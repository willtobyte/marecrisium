namespace {
template <typename T>
void number(lua_State *state, int table, const char *field, T &value) {
  lua_getfield(state, table, field);
  auto valid = 0;
  const auto result = lua_tonumberx(state, -1, &valid);
  if (valid)
    value = static_cast<T>(result);
  lua_pop(state, 1);
}

std::array<SDL_Vertex, 1024> vertices;
constexpr auto indices = [] {
  std::array<int, 1536> values{};
  for (auto i = 0uz; i < values.size() / 6; ++i) {
    const auto vertex = static_cast<int>(i * 4);
    auto *out = values.data() + i * 6;
    out[0] = vertex;
    out[1] = vertex + 1;
    out[2] = vertex + 2;
    out[3] = vertex;
    out[4] = vertex + 2;
    out[5] = vertex + 3;
  }
  return values;
}();
}

int font::render(lua_State *state, font *self, std::string_view text, float x, float y) {
  static std::array<glypheffect, 256> effects;
  std::array<uint64_t, 4> active{};
  auto count = 0uz;

  lua_pushnil(state);
  while (lua_next(state, 5) != 0) {
    auto valid = 0;
    const auto raw = lua_tointegerx(state, -2, &valid);
    if (!valid || !lua_istable(state, -1)) [[unlikely]] {
      lua_pop(state, 1);
      continue;
    }

    const auto index = static_cast<std::size_t>(raw) - 1;
    if (index >= effects.size()) {
      lua_pop(state, 1);
      continue;
    }

    count = std::max(count, index + 1);
    active[index / 64] |= uint64_t{1} << (index % 64);
    auto &effect = effects[index];
    effect = {};
    number(state, -1, "x_offset", effect.x_offset);
    number(state, -1, "y_offset", effect.y_offset);
    number(state, -1, "scale", effect.scale);
    number(state, -1, "angle", effect.angle);
    number(state, -1, "alpha", effect.alpha);
    number(state, -1, "r", effect.r);
    number(state, -1, "g", effect.g);
    number(state, -1, "b", effect.b);

    lua_pop(state, 1);
  }

  self->draw<true>(text, x, y, std::span{effects.data(), count}, active);
  return 0;
}

int font::label(lua_State *state) {
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

  return render(state, self, text, x, y);
}

void font::wire() {
  binding::metatable(L, "Font", nullptr);
  luaL_getmetatable(L, "Font");
  lua_newtable(L);
  binding::callback(L, label);
  lua_setfield(L, -2, "label");
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

static constexpr SDL_FPoint rotate(float x, float y, float middle_x, float middle_y, float cosine, float sine) {
  const auto dx = x - middle_x;
  const auto dy = y - middle_y;
  return {middle_x + dx * cosine - dy * sine, middle_y + dx * sine + dy * cosine};
}

font::font(std::string_view family) {
  const auto filename = std::format("fonts/{}.lua", family);
  const auto meta = io::read(filename);
  const auto chunk = std::format("@{}", filename);
  binding::load(L, meta, chunk);

  binding::call(L, 0, 1);

  const auto config = lua_gettop(L);
  lua_getfield(L, config, "glyphs");
  std::size_t count{};
  const auto *data = lua_isstring(L, -1) ? lua_tolstring(L, -1, &count) : nullptr;
  const auto glyphs = data ? std::string_view{data, count} : std::string_view{};

  number(L, config, "spacing", _spacing);
  number(L, config, "leading", _leading);
  number(L, config, "scale", _scale);

  const auto buffer = io::read(std::format("blobs/fonts/{}.png", family));

  auto spng = std::unique_ptr<spng_ctx, SPNG_Deleter>{spng_ctx_new(SPNG_CTX_IGNORE_ADLER32)};

  spng_set_crc_action(spng.get(), SPNG_CRC_USE, SPNG_CRC_USE);
  spng_set_png_buffer(spng.get(), buffer.data(), buffer.size());

  spng_ihdr ihdr{};
  spng_get_ihdr(spng.get(), &ihdr);

  const auto width = static_cast<int>(ihdr.width);
  const auto height = static_cast<int>(ihdr.height);

  size_t length{};
  spng_decoded_image_size(spng.get(), SPNG_FMT_RGBA8, &length);

  auto decoded = std::make_unique_for_overwrite<uint8_t[]>(length);
  spng_decode_image(spng.get(), decoded.get(), length, SPNG_FMT_RGBA8, SPNG_DECODE_TRNS);

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
      const auto radians = to_radians(angle);
      auto sine = .0f, cosine = .0f;
      sincos(radians, sine, cosine);

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
