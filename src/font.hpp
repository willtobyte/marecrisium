#pragma once

void sincos(float x, float& sine, float& cosine) noexcept;

struct alignas(32) glypheffect final {
  float x_offset{.0f};
  float y_offset{.0f};
  float scale{1.f};
  float angle{.0f};
  float r{1.f};
  float g{1.f};
  float b{1.f};
  float alpha{1.f};
};

struct alignas(32) glyphprops final {
  float u0, v0, u1, v1;
  float width, height;
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
  std::array<glyphprops, 256> _props{};
  std::array<SDL_Vertex, 1024> _vertices{};
  std::array<int, 1536> _indices{};
  std::unique_ptr<SDL_Texture, SDL_Deleter> _texture;
  std::string _glyphs;
  std::size_t _vertex_count{0};
  std::size_t _index_count{0};
  float _fontheight{.0f};
  float _scale{1.f};
  int16_t _spacing{0};
  int16_t _leading{0};
};
