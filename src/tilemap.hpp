#pragma once

struct alignas(16) uv final {
  float u0, v0, u1, v1;
};

class tilemap final {
public:
  tilemap() = default;

  tilemap(std::string_view name, b2WorldId world);

  ~tilemap() = default;

  void draw_background() noexcept;

  void draw_foreground() noexcept;

  [[nodiscard]] int pathfind(lua_State* state, float x1, float y1, float x2, float y2) noexcept;

private:
  std::vector<uint8_t> _collision;

  std::vector<uint32_t> _background_tiles;
  std::vector<uint32_t> _foreground_tiles;

  std::vector<uv> _background_uvs;
  std::vector<uv> _foreground_uvs;

  std::vector<SDL_Vertex> _background_vertices;
  std::vector<int32_t> _background_indices;
  std::vector<SDL_Vertex> _foreground_vertices;
  std::vector<int32_t> _foreground_indices;

  const pixmap* _background_atlas{};
  const pixmap* _foreground_atlas{};

  float _size{};
  float _inv_size{};
  float _vp_x{};
  float _vp_y{};
  float _vp_w{};
  float _vp_h{};

  int32_t _width{};
  int32_t _height{};

  bool _dirty{true};

  void build_layer(const uint32_t* tiles, const uv* uvs, std::vector<SDL_Vertex>& vertices, std::vector<int32_t>& indices) noexcept;
};
