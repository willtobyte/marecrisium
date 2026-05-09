#pragma once

struct uv final {
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

  void draw_background();

  void draw_foreground();

private:
  friend class minimap;

  std::vector<uint8_t> _collision;

  layer _background;
  layer _foreground;

  float _size{};
  float _inverse{};
  struct viewport _snapshot{};

  int32_t _width{};
  int32_t _height{};

  bool _dirty{true};

  void tessellate(layer& layer);
};
