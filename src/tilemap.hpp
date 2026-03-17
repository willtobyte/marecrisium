#pragma once

struct alignas(16) uv final {
  float u0, v0, u1, v1;
};

class tilemap final {
public:
  struct layer final {
    const pixmap* atlas{};
    std::vector<SDL_Vertex> vertices;
    std::vector<int32_t> indices;
    std::vector<uint32_t> tiles;
    std::vector<uv> uvs;
  };

  tilemap() = default;

  tilemap(std::string_view name, b2WorldId world);

  ~tilemap() = default;

  void draw_background() noexcept;

  void draw_foreground() noexcept;

  [[nodiscard]] int pathfind(lua_State* state, float x1, float y1, float x2, float y2) noexcept;

private:
  std::vector<uint8_t> _collision;

  layer _background;
  layer _foreground;

  float _size{};
  float _inverse_size{};
  float _viewport_x{};
  float _viewport_y{};
  float _viewport_width{};
  float _viewport_height{};

  int32_t _width{};
  int32_t _height{};

  bool _dirty{true};

  void build_layer(layer& layer) noexcept;
};
