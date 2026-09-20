#pragma once

class label final {
public:
  label(font *font, float x, float y, float w, float h);

  static void wire();

  void draw(std::string_view text) const;

  void draw(std::string_view text, std::span<const glypheffect> effects) const;

  template <bool sparse>
  void draw(std::string_view text, std::span<const glypheffect> effects, std::span<const uint64_t> active) const;

private:
  font *_font;
  float _x;
  float _y;
  float _w;
  float _h;
};
