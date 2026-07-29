pixmap::pixmap(std::string_view filename) {
  const auto buffer = io::read(filename);

  auto pixels = std::unique_ptr<stbi_uc, STBI_Deleter>{stbi_load_from_memory(
    buffer.data(), static_cast<int>(buffer.size()), &_width, &_height, nullptr, STBI_rgb_alpha)};

  _texture = std::unique_ptr<SDL_Texture, SDL_Deleter>{
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, _width, _height)};

  SDL_UpdateTexture(_texture.get(), nullptr, pixels.get(), _width * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_RGBA32));
  SDL_SetTextureScaleMode(_texture.get(), SDL_SCALEMODE_NEAREST);
  SDL_SetTextureBlendMode(_texture.get(), SDL_BLENDMODE_BLEND);
}

void pixmap::draw(
    const float sx, const float sy, const float sw, const float sh,
    const float dx, const float dy, const float dw, const float dh,
    const double angle,
    const uint8_t alpha,
    const mirror flip
) const {
  const SDL_FRect source{sx, sy, sw, sh};
  const SDL_FRect destination{dx, dy, dw, dh};

  SDL_SetTextureAlphaMod(_texture.get(), alpha);
  SDL_RenderTextureRotated(renderer, _texture.get(), &source, &destination, angle, nullptr, static_cast<SDL_FlipMode>(flip));
}

pixmap::operator SDL_Texture*() const {
  return _texture.get();
}

int pixmap::width() const {
  return _width;
}

int pixmap::height() const {
  return _height;
}
