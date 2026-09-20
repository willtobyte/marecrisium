namespace {
template <typename T>
void number(lua_State *state, int table, const char *field, T &value, T fallback = {}) {
  lua_getfield(state, table, field);

  int valid;
  const auto result = lua_tonumberx(state, -1, &valid);

  value = valid ? static_cast<T>(result) : fallback;

  lua_pop(state, 1);
}

template <typename T>
void number(lua_State *state, int table, const char *field, T &value, T fallback, T minimum, T maximum) {
  lua_getfield(state, table, field);

  int valid;
  const auto result = lua_tonumberx(state, -1, &valid);

  value = valid ? std::clamp(static_cast<T>(result), minimum, maximum) : fallback;

  lua_pop(state, 1);
}

constexpr auto capacity = 256uz;
std::array<SDL_Vertex, capacity * 4> vertices;
static consteval auto triangulate() {
  std::array<int, capacity * 6> values{};
  for (auto quad = 0uz; quad < capacity; ++quad) {
    const auto first = static_cast<int>(quad * 4);

    auto *out = values.data() + quad * 6;

    out[0] = first;
    out[1] = first + 1;
    out[2] = first + 2;
    out[3] = first;
    out[4] = first + 2;
    out[5] = first + 3;
  }

  return values;
}

static constexpr auto indices = triangulate();
}

static constexpr SDL_FPoint rotate(float x, float y, float middle_x, float middle_y, float cosine, float sine) {
  const auto dx = x - middle_x;
  const auto dy = y - middle_y;

  return {middle_x + dx * cosine - dy * sine, middle_y + dx * sine + dy * cosine};
}

font::font(std::string_view family) {
  const auto chunk = std::format("@fonts/{}.lua", family);
  const auto filename = std::string_view{chunk}.substr(1);
  const auto source = io::read(filename);
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  const auto top = lua_gettop(L);
  if (pcall(L, 0, 1) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  lua_getfield(L, top, "glyphs");

  std::size_t length;
  const auto* data = lua_tolstring(L, -1, &length);
  const std::string_view glyphs{data, length};

  number(L, top, "spacing", _spacing);
  number(L, top, "leading", _leading);
  number(L, top, "scale", _scale, 1.f);

  const auto buffer = io::read(std::format("blobs/fonts/{}.png", family));

  int width, height;
  auto decoded = std::unique_ptr<stbi_uc, STBI_Deleter>{stbi_load_from_memory(
    buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha)};

  _texture = std::unique_ptr<SDL_Texture, SDL_Deleter>{
    SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height)};

  SDL_UpdateTexture(_texture.get(), nullptr, decoded.get(), width * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_RGBA32));
  SDL_SetTextureScaleMode(_texture.get(), SDL_SCALEMODE_NEAREST);
  SDL_SetTextureBlendMode(_texture.get(), SDL_BLENDMODE_BLEND);

  const auto* pixels = reinterpret_cast<const uint32_t*>(decoded.get());
  const auto separator = pixels[0];

  const auto iw = 1.f / width;
  const auto ih = 1.f / height;

  auto x = 0, y = 0;
  auto first = true;
  for (char glyph : glyphs) {
    while (x < width && pixels[y * width + x] == separator) {
      ++x;
    }

    assert(x < width && "glyph must fit within the atlas width");

    auto w = 0;
    while (x + w < width && pixels[y * width + x + w] != separator) {
      ++w;
    }

    auto h = 0;
    while (y + h < height && pixels[(y + h) * width + x] != separator) {
      ++h;
    }

    const auto fx = x;
    const auto fy = y;
    const auto fw = w;
    const auto fh = h;

    _props[static_cast<uint8_t>(glyph)] = {
      fx * iw,
      fy * ih,
      (fx + fw) * iw,
      (fy + fh) * ih,
      fw * _scale,
      fh * _scale
    };

    if (first) {
      _fontheight = fh * _scale;
      first = false;
    }

    x += w;
  }

  lua_pop(L, 2);
}

void font::draw(std::string_view text, float x, float y) const {
  draw(text, x, y, {});
}

void font::draw(std::string_view text, float x, float y, std::span<const glypheffect> effects) const {
  draw<false>(text, x, y, effects, {});
}

void font::draw(std::string_view text, float x, float y, float w, float h) const {
  draw(text, x, y, w, h, {});
}

void font::draw(
  std::string_view text,
  float x,
  float y,
  float w,
  float h,
  std::span<const glypheffect> effects) const {
  draw<false>(text, x, y, w, h, effects, {});
}

template <bool sparse>
void font::draw(
  std::string_view text,
  float x,
  float y,
  float w,
  float h,
  std::span<const glypheffect> effects,
  std::span<const uint64_t> active) const {
  if (w <= .0f) [[unlikely]] return;

  const auto *mask = active.data();
  if constexpr (sparse) {
    const auto size = active.size();

    assert(size == 4 && "glyph effect mask must have four words");
    [[assume(size == 4)]];
  }

  const auto right = x + w;
  const auto bottom = y + h;
  const auto line = _fontheight + _leading;
  const auto space = _props[static_cast<uint8_t>(' ')].width;
  const auto n = text.size();
  auto p = 0uz;
  auto src = 0uz;
  auto drawn = 0uz;
  auto cx = x;
  auto cy = y;

  while (p < n) {
    const auto c = text[p];

    if (c == '\n') {
      cx = x;
      cy += line;
      ++p;

      if (cy + _fontheight > bottom) break;

      continue;
    }

    if (c == ' ') {
      if (cx == x) {
        ++p;
        ++src;

        continue;
      }

      auto q = p;
      while (q < n && text[q] == ' ') ++q;

      auto r = q;
      while (r < n && text[r] != ' ' && text[r] != '\n') ++r;

      auto gap = .0f;
      for (auto k = p; k < q; ++k) gap += space + _spacing;

      auto word = .0f;
      if (r > q) {
        for (auto k = q; k < r; ++k) word += _props[static_cast<uint8_t>(text[k])].width + _spacing;
        word -= _spacing;
      }

      if (r > q && cx + gap + word > right) {
        cx = x;
        cy += line;

        if (cy + _fontheight > bottom) break;

        src += q - p;
        p = q;

        continue;
      }

      if (cx + space > right) {
        cx = x;
        cy += line;

        if (cy + _fontheight > bottom) break;

        src += q - p;
        p = q;

        continue;
      }

      if (cy + _fontheight > bottom) break;
    } else {
      auto s = p;
      while (s > 0 && text[s - 1] != ' ' && text[s - 1] != '\n') --s;

      auto e = p;
      while (e < n && text[e] != ' ' && text[e] != '\n') ++e;

      auto word = .0f;
      for (auto k = s; k < e; ++k) word += _props[static_cast<uint8_t>(text[k])].width + _spacing;
      word -= _spacing;

      if (cx != x && p == s && cx + word > right) {
        cx = x;
        cy += line;

        if (cy + _fontheight > bottom) break;

        continue;
      }

      const auto width = _props[static_cast<uint8_t>(c)].width;

      if (cx + width > right && cx != x) {
        cx = x;
        cy += line;

        if (cy + _fontheight > bottom) break;

        continue;
      }

      if (cy + _fontheight > bottom) break;
    }

    if (drawn == _props.size()) [[unlikely]] break;

    const auto &glyph = _props[static_cast<uint8_t>(c)];
    auto gx = cx;
    auto gy = cy;
    auto sw = glyph.width;
    auto sh = glyph.height;
    auto color = SDL_FColor{1.f, 1.f, 1.f, 1.f};
    auto angle = .0f;

    if (src < effects.size() &&
        (!sparse || mask[src / 64] & (uint64_t{1} << (src % 64)))) {
      const auto &effect = effects[src];

      gx += effect.x_offset;
      gy += effect.y_offset;
      sw *= effect.scale;
      sh *= effect.scale;
      angle = effect.angle;
      color = {effect.r, effect.g, effect.b, effect.alpha};
    }

    auto *out = vertices.data() + drawn * 4;

    if (angle == .0f) [[likely]] {
      out[0] = SDL_Vertex{{gx, gy}, color, {glyph.u0, glyph.v0}};
      out[1] = SDL_Vertex{{gx + sw, gy}, color, {glyph.u1, glyph.v0}};
      out[2] = SDL_Vertex{{gx + sw, gy + sh}, color, {glyph.u1, glyph.v1}};
      out[3] = SDL_Vertex{{gx, gy + sh}, color, {glyph.u0, glyph.v1}};
    } else {
      const auto midx = gx + sw * .5f;
      const auto midy = gy + sh * .5f;

      float sine, cosine;
      sincos(angle * (std::numbers::pi_v<float> / 180.f), sine, cosine);

      out[0] = SDL_Vertex{rotate(gx, gy, midx, midy, cosine, sine), color, {glyph.u0, glyph.v0}};
      out[1] = SDL_Vertex{rotate(gx + sw, gy, midx, midy, cosine, sine), color, {glyph.u1, glyph.v0}};
      out[2] = SDL_Vertex{rotate(gx + sw, gy + sh, midx, midy, cosine, sine), color, {glyph.u1, glyph.v1}};
      out[3] = SDL_Vertex{rotate(gx, gy + sh, midx, midy, cosine, sine), color, {glyph.u0, glyph.v1}};
    }

    cx += glyph.width + _spacing;
    ++p;
    ++src;
    ++drawn;
  }

  if (drawn == 0) [[unlikely]] return;

  SDL_RenderGeometry(
    renderer,
    _texture.get(),
    vertices.data(),
    static_cast<int>(drawn * 4),
    indices.data(),
    static_cast<int>(drawn * 6)
  );
}

template <bool sparse>
void font::draw(std::string_view text, float x, float y, std::span<const glypheffect> effects, std::span<const uint64_t> active) const {
  if (text.empty()) [[unlikely]] return;

  const auto *mask = active.data();
  if constexpr (sparse) {
    const auto size = active.size();

    assert(size == 4 && "glyph effect mask must have four words");
    [[assume(size == 4)]];
  }

  auto cx = x;
  auto cy = y;
  auto count = 0uz;
  for (const auto character : text) {
    if (character == '\n') {
      cx = x;
      cy += _fontheight + _leading;

      continue;
    }

    if (count == _props.size()) [[unlikely]] break;

    const auto &glyph = _props[static_cast<uint8_t>(character)];
    auto gx = cx;
    auto gy = cy;
    auto sw = glyph.width;
    auto sh = glyph.height;
    auto color = SDL_FColor{1.f, 1.f, 1.f, 1.f};
    auto angle = .0f;

    if (count < effects.size() &&
        (!sparse || mask[count / 64] & (uint64_t{1} << (count % 64)))) {
      const auto &effect = effects[count];

      gx += effect.x_offset;
      gy += effect.y_offset;
      sw *= effect.scale;
      sh *= effect.scale;
      angle = effect.angle;
      color = {effect.r, effect.g, effect.b, effect.alpha};
    }

    auto *out = vertices.data() + count * 4;
    if (angle == .0f) [[likely]] {
      out[0] = SDL_Vertex{{gx, gy}, color, {glyph.u0, glyph.v0}};
      out[1] = SDL_Vertex{{gx + sw, gy}, color, {glyph.u1, glyph.v0}};
      out[2] = SDL_Vertex{{gx + sw, gy + sh}, color, {glyph.u1, glyph.v1}};
      out[3] = SDL_Vertex{{gx, gy + sh}, color, {glyph.u0, glyph.v1}};
    } else {
      const auto midx = gx + sw * .5f;
      const auto midy = gy + sh * .5f;

      float sine, cosine;
      sincos(angle * (std::numbers::pi_v<float> / 180.f), sine, cosine);

      out[0] = SDL_Vertex{rotate(gx, gy, midx, midy, cosine, sine), color, {glyph.u0, glyph.v0}};
      out[1] = SDL_Vertex{rotate(gx + sw, gy, midx, midy, cosine, sine), color, {glyph.u1, glyph.v0}};
      out[2] = SDL_Vertex{rotate(gx + sw, gy + sh, midx, midy, cosine, sine), color, {glyph.u1, glyph.v1}};
      out[3] = SDL_Vertex{rotate(gx, gy + sh, midx, midy, cosine, sine), color, {glyph.u0, glyph.v1}};
    }

    cx += glyph.width + _spacing;
    ++count;
  }

  if (count == 0) [[unlikely]] return;

  SDL_RenderGeometry(
    renderer,
    _texture.get(),
    vertices.data(),
    static_cast<int>(count * 4),
    indices.data(),
    static_cast<int>(count * 6)
  );
}

template void font::draw<true>(
  std::string_view,
  float,
  float,
  float,
  float,
  std::span<const glypheffect>,
  std::span<const uint64_t>) const;
