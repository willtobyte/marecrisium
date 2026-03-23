#include "font.hpp"

[[nodiscard]] static SDL_FPoint rotate(float px, float py, float midx, float midy, float cosine, float sine) noexcept {
  const auto dx = px - midx;
  const auto dy = py - midy;
  return {midx + dx * cosine - dy * sine, midy + dx * sine + dy * cosine};
}

font::font(std::string_view family) {
  const auto filename = std::format("overlay/fonts/{}.lua", family);
  const auto meta = io::read(filename);
  const auto label = std::format("@{}", filename);
  compile(L, meta, label);

  pcall(L, 0, 1);

  _glyphs = property<std::string_view>(L, -1, "glyphs");
  _spacing = static_cast<int16_t>(property<int>(L, -1, "spacing", _spacing));
  _leading = static_cast<int16_t>(property<int>(L, -1, "leading", _leading));
  _scale = property<float>(L, -1, "scale", _scale);

  lua_pop(L, 1);

  const auto buffer = io::read(std::format("blobs/overlay/fonts/{}.png", family));

  auto spng = std::unique_ptr<spng_ctx, SPNG_Deleter>{spng_ctx_new(SPNG_CTX_IGNORE_ADLER32)};

  spng_set_crc_action(spng.get(), SPNG_CRC_USE, SPNG_CRC_USE);
  spng_set_png_buffer(spng.get(), buffer.data(), buffer.size());

  spng_ihdr ihdr;
  spng_get_ihdr(spng.get(), &ihdr);

  const auto width = static_cast<int>(ihdr.width);
  const auto height = static_cast<int>(ihdr.height);

  size_t length;
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

    assert(x < width);

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

void font::draw(std::string_view text, float x, float y) noexcept {
  draw(text, x, y, {});
}

void font::draw(std::string_view text, float x, float y, std::span<const glypheffect> effects) noexcept {
  if (text.empty()) [[unlikely]] return;

  _vertex_count = 0;
  _index_count = 0;

  auto cx = x;
  auto cy = y;
  auto gi = 0uz;

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
    auto sw = glyph.sw;
    auto sh = glyph.sh;
    auto color = SDL_FColor{1.f, 1.f, 1.f, 1.f};

    auto angle = .0f;

    if (gi < effects.size()) {
      const auto& effect = effects[gi];

      gx += effect.xoffset;
      gy += effect.yoffset;
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

    _indices[_index_count++] = base;
    _indices[_index_count++] = base + 1;
    _indices[_index_count++] = base + 2;
    _indices[_index_count++] = base;
    _indices[_index_count++] = base + 2;
    _indices[_index_count++] = base + 3;

    cx += glyph.sw + static_cast<float>(_spacing);
    ++gi;
  }

  if (_vertex_count == 0) [[unlikely]] return;

  SDL_RenderGeometry(
    renderer,
    _texture.get(),
    _vertices.data(),
    static_cast<int>(_vertex_count),
    _indices.data(),
    static_cast<int>(_index_count)
  );
}
