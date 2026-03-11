#pragma once

void sincos(float x, float& osin, float& ocos) noexcept;

struct glypheffect final {
  float xoffset{.0f};
  float yoffset{.0f};
  float scale{1.f};
  float angle{.0f};
  float r{1.f};
  float g{1.f};
  float b{1.f};
  float alpha{1.f};
};

struct alignas(32) glyphprops final {
  float u0, v0, u1, v1;
  float sw, sh;
  float w;
};

class font final {
public:
  font() = delete;

  explicit font(std::string_view family);

  ~font() = default;

  font(font&&) noexcept = default;
  font& operator=(font&&) noexcept = default;

  void draw(std::string_view text, float x, float y) noexcept;

  void draw(std::string_view text, float x, float y, std::span<const glypheffect> effects) noexcept;

private:
  std::unique_ptr<SDL_Texture, SDL_Deleter> _texture;
  int _width{0};
  int _height{0};
  int16_t _spacing{0};
  int16_t _leading{0};
  float _fontheight{.0f};
  float _scale{1.f};
  std::string _glyphs;
  std::array<glyphprops, 256> _props{};
  std::array<SDL_Vertex, 1024> _vertices{};
  std::size_t _vertex_count{0};
  std::array<int, 1536> _indices{};
  std::size_t _index_count{0};
};
