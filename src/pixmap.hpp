#pragma once

class pixmap final {
public:
  pixmap() = delete;
  explicit pixmap(std::string_view filename);
  ~pixmap() = default;

  void draw(
    const float sx, const float sy, const float sw, const float sh,
    const float dx, const float dy, const float dw, const float dh,
    const double angle = .0,
    const uint8_t alpha = 255,
    const mirror flip = mirror::none
  ) const;

  operator SDL_Texture*() const;

  int width() const;
  int height() const;

private:
  int _width;
  int _height;

  std::unique_ptr<SDL_Texture, SDL_Deleter> _texture;
};
