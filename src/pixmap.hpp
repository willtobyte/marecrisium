#pragma once

class pixmap final {
public:
  explicit pixmap(std::string_view filename);
  void draw(
    const float sx, const float sy, const float sw, const float sh,
    const float dx, const float dy, const float dw, const float dh,
    const double angle = .0,
    const uint8_t alpha = 255,
    const mirror::value mirror = mirror::value::none
  ) const;

  void draw(const float dx, const float dy, const float dw, const float dh) const;

  operator SDL_Texture*() const;

  int width() const { return _width; }
  int height() const { return _height; }

private:
  int _width;
  int _height;

  std::unique_ptr<SDL_Texture, SDL_Deleter> _texture;
};
