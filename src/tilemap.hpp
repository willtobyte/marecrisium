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

  void draw_background() noexcept;

  void draw_foreground() noexcept;

  int pathfind(lua_State* state, float x1, float y1, float x2, float y2, float radius) noexcept;

private:
  friend class minimap;

  struct node final {
    float f;
    int32_t index;
  };

  struct pathfinder final {
    std::vector<float> g;
    std::vector<uint32_t> generation;
    std::vector<int32_t> parent;
    std::vector<int32_t> path;
    std::vector<node> heap;
    uint32_t current_generation{};
  };

  std::vector<uint8_t> _collision;
  pathfinder _pathfinder;
  b2WorldId _world{};

  layer _background;
  layer _foreground;

  float _size{};
  float _inverse{};
  struct viewport _viewport_snapshot{};

  int32_t _width{};
  int32_t _height{};

  bool _dirty{true};

  void tessellate(layer& layer) noexcept;
};
