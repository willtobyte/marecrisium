#pragma once

struct alignas(16) uv final {
  float u0, v0, u1, v1;
};

class tilemap final {
public:
  tilemap() = default;

  tilemap(std::string_view name, b2WorldId world);

  ~tilemap() = default;

  void set_camera(float x, float y, float w, float h) noexcept;

  void draw_background() noexcept;

  void draw_foreground() noexcept;

private:
  int32_t _width{};
  int32_t _height{};
  float _size{};
  float _inv_size{};

  std::vector<uint32_t> _background_tiles;
  std::vector<uint32_t> _foreground_tiles;

  std::vector<uv> _background_uvs;
  std::vector<uv> _foreground_uvs;

  const pixmap* _background_atlas{};
  const pixmap* _foreground_atlas{};

  std::vector<SDL_Vertex> _background_vertices;
  std::vector<int32_t> _background_indices;
  std::vector<SDL_Vertex> _foreground_vertices;
  std::vector<int32_t> _foreground_indices;

  float _camera_x{};
  float _camera_y{};
  float _camera_w{};
  float _camera_h{};
  bool _dirty{true};

  void build_layer(const uint32_t* tiles, const uv* uvs, std::vector<SDL_Vertex>& vertices, std::vector<int32_t>& indices) noexcept;
};
