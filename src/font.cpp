static int font_label(lua_State *state) {
  auto *self = *static_cast<font **>(luaL_checkudata(state, 1, "Font"));
  std::size_t length{};
  const auto *text = luaL_checklstring(state, 2, &length);
  const auto x = static_cast<float>(luaL_checknumber(state, 3));
  const auto y = static_cast<float>(luaL_checknumber(state, 4));

  if (!lua_istable(state, 5)) [[likely]] {
    self->draw({text, length}, x, y);
    return 0;
  }

  std::array<glypheffect, 256> effects{};
  auto count = 0uz;

  lua_pushnil(state);
  while (lua_next(state, 5) != 0) {
    if (!lua_isnumber(state, -2) || !lua_istable(state, -1)) [[unlikely]] {
      lua_pop(state, 1);
      continue;
    }

    const auto index = static_cast<std::size_t>(lua_tointeger(state, -2)) - 1;

    if (index >= effects.size()) {
      lua_pop(state, 1);
      continue;
    }

    auto &effect = effects[index];

    lua_getfield(state, -1, "x_offset");
    if (lua_isnumber(state, -1))
      effect.x_offset = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "y_offset");
    if (lua_isnumber(state, -1))
      effect.y_offset = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "scale");
    if (lua_isnumber(state, -1))
      effect.scale = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "angle");
    if (lua_isnumber(state, -1))
      effect.angle = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "alpha");
    if (lua_isnumber(state, -1))
      effect.alpha = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "r");
    if (lua_isnumber(state, -1))
      effect.r = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "g");
    if (lua_isnumber(state, -1))
      effect.g = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "b");
    if (lua_isnumber(state, -1))
      effect.b = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    if (index >= count) count = index + 1;

    lua_pop(state, 1);
  }

  self->draw({text, length}, x, y, std::span{effects.data(), count});
  return 0;
}

namespace {
  namespace property {
    constexpr auto label = "label"_hs;
  }

  int _label_reference = LUA_NOREF;
}

static int font_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == property::label) [[likely]] {
    lua_rawgeti(state, LUA_REGISTRYINDEX, _label_reference);
    return 1;
  }

  return lua_pushnil(state), 1;
}

void font::wire() {
  cfunction(L, font_label);
  _label_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  metatable(L, "Font", font_index);
}

static SDL_FPoint rotate(float x, float y, float middle_x, float middle_y, float cosine, float sine) {
  const auto dx = x - middle_x;
  const auto dy = y - middle_y;
  return {middle_x + dx * cosine - dy * sine, middle_y + dx * sine + dy * cosine};
}

font::font(std::string_view family) {
  const auto filename = std::format("fonts/{}.lua", family);
  const auto meta = io::read(filename);
  const auto chunk = std::format("@{}", filename);
  compile(L, meta, chunk);

  pcall(L, 0, 1);

  lua_getfield(L, -1, "glyphs");
  _glyphs = lua_isstring(L, -1) ? lua_tostring(L, -1) : std::string{};
  lua_pop(L, 1);

  lua_getfield(L, -1, "spacing");
  if (lua_isnumber(L, -1))
    _spacing = static_cast<int16_t>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "leading");
  if (lua_isnumber(L, -1))
    _leading = static_cast<int16_t>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "scale");
  if (lua_isnumber(L, -1))
    _scale = static_cast<float>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_pop(L, 1);

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
  for (char glyph : _glyphs) {
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
}

void font::draw(std::string_view text, float x, float y) {
  draw(text, x, y, {});
}

void font::draw(std::string_view text, float x, float y, std::span<const glypheffect> effects) {
  if (text.empty()) [[unlikely]] return;

  _vertex_count = 0;
  _index_count = 0;

  auto cx = x;
  auto cy = y;
  auto ei = 0uz;
  for (const auto character : text) {
    if (character == '\n') {
      cx = x;
      cy += _fontheight + static_cast<float>(_leading);
      continue;
    }

    if (_vertex_count + 4 > _vertices.size()) break;

    const auto& glyph = _props[static_cast<uint8_t>(character)];

    const auto base = static_cast<int>(_vertex_count);

    auto gx = cx;
    auto gy = cy;
    auto sw = glyph.width;
    auto sh = glyph.height;
    auto color = SDL_FColor{1.f, 1.f, 1.f, 1.f};

    auto angle = .0f;

    if (ei < effects.size()) {
      const auto& effect = effects[ei];

      gx += effect.x_offset;
      gy += effect.y_offset;
      sw *= effect.scale;
      sh *= effect.scale;
      angle = effect.angle;
      color = {effect.r, effect.g, effect.b, effect.alpha};
    }

    if (angle == .0f) [[likely]] {
      _vertices[_vertex_count++] = SDL_Vertex{{gx, gy}, color, {glyph.u0, glyph.v0}};
      _vertices[_vertex_count++] = SDL_Vertex{{gx + sw, gy}, color, {glyph.u1, glyph.v0}};
      _vertices[_vertex_count++] = SDL_Vertex{{gx + sw, gy + sh}, color, {glyph.u1, glyph.v1}};
      _vertices[_vertex_count++] = SDL_Vertex{{gx, gy + sh}, color, {glyph.u0, glyph.v1}};
    } else {
      const auto midx = gx + sw * .5f;
      const auto midy = gy + sh * .5f;
      const auto radians = to_radians(angle);
      auto sine = .0f, cosine = .0f;
      sincos(radians, sine, cosine);

      _vertices[_vertex_count++] = SDL_Vertex{rotate(gx, gy, midx, midy, cosine, sine), color, {glyph.u0, glyph.v0}};
      _vertices[_vertex_count++] = SDL_Vertex{rotate(gx + sw, gy, midx, midy, cosine, sine), color, {glyph.u1, glyph.v0}};
      _vertices[_vertex_count++] = SDL_Vertex{rotate(gx + sw, gy + sh, midx, midy, cosine, sine), color, {glyph.u1, glyph.v1}};
      _vertices[_vertex_count++] = SDL_Vertex{rotate(gx, gy + sh, midx, midy, cosine, sine), color, {glyph.u0, glyph.v1}};
    }

    auto* ip = _indices.data() + _index_count;
    *ip++ = base; *ip++ = base + 1; *ip++ = base + 2;
    *ip++ = base; *ip++ = base + 2; *ip++ = base + 3;
    _index_count += 6;

    cx += sw + static_cast<float>(_spacing);
    ++ei;
  }

  SDL_RenderGeometry(
    renderer,
    _texture.get(),
    _vertices.data(),
    static_cast<int>(_vertex_count),
    _indices.data(),
    static_cast<int>(_index_count)
  );
}
