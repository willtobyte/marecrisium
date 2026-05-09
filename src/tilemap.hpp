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

  int pathfind(lua_State* state, float x1, float y1, float x2, float y2, float radius);

private:
  friend class minimap;
#ifdef DEBUG
  friend class stage;
#endif

  struct node final {
    float f;
    int32_t index;
    uint32_t tiebreak;
  };

  static bool greater(const node& a, const node& b) noexcept;

  struct pathfinder final {
    std::vector<float> g;
    std::vector<uint32_t> generations;
    std::vector<int32_t> parent;
    std::vector<int32_t> path;
    std::vector<node> heap;
    uint32_t generation{};
    uint32_t tiebreak{};
  };

  std::vector<uint8_t> _collision;
  std::vector<uint16_t> _components; // 0xFFFF == blocked
  std::array<std::vector<int16_t>, 8> _jump; // 8 directions × cell, JPS+ jump table

  pathfinder _pathfinder;
  b2WorldId _world{};

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
