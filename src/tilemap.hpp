#pragma once

class pixmappool;

class tilemap final {
public:
  tilemap(std::string_view name, pixmappool& pixmaps, b2WorldId world);
  ~tilemap();

  void draw_background(const camera& camera) const noexcept;

  void draw_foreground(const camera& camera) const noexcept;

private:
  const pixmap* _tileset{};
  int _tile{};
  int _columns{};
  int _rows{};
  int _tileset_columns{};

  std::vector<std::array<float, 4>> _uv_table;

  std::vector<SDL_Vertex> _background_vertices;
  std::vector<int> _background_indices;
  std::vector<SDL_Vertex> _foreground_vertices;
  std::vector<int> _foreground_indices;

  b2BodyId _collision_body{b2_nullBodyId};
  std::vector<b2ShapeId> _collision_shapes;

  void build_layer(
    const std::vector<uint16_t>& tiles,
    std::vector<SDL_Vertex>& vertices,
    std::vector<int>& indices
  ) const;

  void build_collision(const std::vector<uint8_t>& collision, b2WorldId world);

  static void draw_layer(
    const pixmap* tileset,
    const std::vector<SDL_Vertex>& vertices,
    const std::vector<int>& indices,
    const camera& camera
  ) noexcept;
};
