namespace {
template <typename T>
void number(lua_State *state, int table, const char *field, T &value, T fallback = {}) {
  lua_getfield(state, table, field);
  auto valid = 0;
  const auto result = lua_tonumberx(state, -1, &valid);
  value = valid ? static_cast<T>(result) : fallback;
  lua_pop(state, 1);
}

std::array<SDL_Vertex, 1024> vertices;
static consteval auto triangulate() {
  std::array<int, 1536> values{};
  for (auto index = 0uz; index < values.size() / 6; ++index) {
    const auto vertex = static_cast<int>(index * 4);
    auto *out = values.data() + index * 6;
    out[0] = vertex;
    out[1] = vertex + 1;
    out[2] = vertex + 2;
    out[3] = vertex;
    out[4] = vertex + 2;
    out[5] = vertex + 3;
  }

  return values;
}

constexpr auto indices = triangulate();
}

static int label_callback(lua_State *state) {
  auto *self = *static_cast<font **>(luaL_checkudata(state, 1, "Font"));
  std::size_t length{};
  const auto *data = luaL_checklstring(state, 2, &length);
  const auto text = std::string_view{data, length};
  const auto x = static_cast<float>(luaL_checknumber(state, 3));
  const auto y = static_cast<float>(luaL_checknumber(state, 4));

  if (!lua_istable(state, 5)) [[likely]] {
    self->draw(text, x, y);
    return 0;
  }

  static std::array<glypheffect, 256> effects;
  std::array<uint64_t, 4> active{};
  auto count = 0uz;

  for (lua_pushnil(state); lua_next(state, 5) != 0; lua_pop(state, 1)) {
    const auto raw = lua_tointeger(state, -2);
    const auto valid = raw > 0 && raw <= static_cast<lua_Integer>(effects.size());
    assert(valid && "glyph effect index must be valid");
    [[assume(valid)]];

    const auto index = static_cast<std::size_t>(raw) - 1;

    active[index / 64] |= uint64_t{1} << (index % 64);

    auto &effect = effects[index];
    number(state, -1, "x_offset", effect.x_offset);
    number(state, -1, "y_offset", effect.y_offset);
    number(state, -1, "scale", effect.scale, 1.f);
    number(state, -1, "angle", effect.angle);
    number(state, -1, "alpha", effect.alpha, 1.f);
    number(state, -1, "r", effect.r, 1.f);
    number(state, -1, "g", effect.g, 1.f);
    number(state, -1, "b", effect.b, 1.f);

    effect.alpha = std::clamp(effect.alpha, .0f, 1.f);
    effect.r = std::clamp(effect.r, .0f, 1.f);
    effect.g = std::clamp(effect.g, .0f, 1.f);
    effect.b = std::clamp(effect.b, .0f, 1.f);

    count = std::max(count, index + 1);
  }

  self->draw<true>(text, x, y, std::span{effects.data(), count}, active);
  return 0;
}

void font::wire() {
  luaL_newmetatable(L, "Font");
  lua_pushliteral(L, "Font");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, label_callback);
  lua_setfield(L, -2, "label");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

static constexpr SDL_FPoint rotate(float x, float y, float middle_x, float middle_y, float cosine, float sine) {
  const auto dx = x - middle_x;
  const auto dy = y - middle_y;
  return {middle_x + dx * cosine - dy * sine, middle_y + dx * sine + dy * cosine};
}

font::font(std::string_view family) {
  const auto chunk = std::format("@fonts/{}.lua", family);
  const auto path = std::string_view{chunk}.substr(1);
  const auto source = io::read(path);
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]] {
    lua_error(L);
    std::unreachable();
  }

  const auto base = lua_gettop(L);
  lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
  lua_insert(L, base);
  const auto status = lua_pcall(L, 0, 1, base);
  lua_remove(L, base);
  if (status != LUA_OK) [[unlikely]] {
    lua_error(L);
    std::unreachable();
  }

  const auto config = lua_gettop(L);
  lua_getfield(L, config, "glyphs");
  std::size_t count{};
  const auto *data = lua_isstring(L, -1) ? lua_tolstring(L, -1, &count) : nullptr;
  const auto glyphs = data ? std::string_view{data, count} : std::string_view{};

  number(L, config, "spacing", _spacing);
  number(L, config, "leading", _leading);
  number(L, config, "scale", _scale, 1.f);

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

  const auto iw = 1.f / static_cast<float>(width);
  const auto ih = 1.f / static_cast<float>(height);

  auto x = 0, y = 0;
  auto first = true;
  for (char glyph : glyphs) {
    while (x < width && pixels[y * width + x] == separator) {
      ++x;
    }

    assert(x < width && "ran past atlas width while scanning for next glyph");

    auto w = 0;
    while (x + w < width && pixels[y * width + x + w] != separator) {
      ++w;
    }

    auto h = 0;
    while (y + h < height && pixels[(y + h) * width + x] != separator) {
      ++h;
    }

    const auto fx = static_cast<float>(x);
    const auto fy = static_cast<float>(y);
    const auto fw = static_cast<float>(w);
    const auto fh = static_cast<float>(h);

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
      cy += _fontheight + static_cast<float>(_leading);
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
      const auto radians = angle * (std::numbers::pi_v<float> / 180.f);
      const auto sine = std::sin(radians);
      const auto cosine = std::cos(radians);

      out[0] = SDL_Vertex{rotate(gx, gy, midx, midy, cosine, sine), color, {glyph.u0, glyph.v0}};
      out[1] = SDL_Vertex{rotate(gx + sw, gy, midx, midy, cosine, sine), color, {glyph.u1, glyph.v0}};
      out[2] = SDL_Vertex{rotate(gx + sw, gy + sh, midx, midy, cosine, sine), color, {glyph.u1, glyph.v1}};
      out[3] = SDL_Vertex{rotate(gx, gy + sh, midx, midy, cosine, sine), color, {glyph.u0, glyph.v1}};
    }

    cx += sw + static_cast<float>(_spacing);
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
