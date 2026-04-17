#pragma once

class tilemap;

class minimap final {
public:
  minimap() = delete;

  minimap(const tilemap& tilemap, entt::registry& registry,
          color solid, color passable, color empty,
          color player, color entity);

  ~minimap() = default;

  minimap(minimap&&) noexcept = default;
  minimap& operator=(minimap&&) noexcept = default;

  void draw() noexcept;

  bool _visible{false};

  static void wire();

private:
  static constexpr int32_t RADIUS = 64;
  static constexpr int32_t SIDE = RADIUS * 2 + 1;
  static constexpr float SIZE = 310.f;

  const tilemap* _tilemap;
  entt::registry* _registry;

  uint32_t _solid;
  uint32_t _passable;
  uint32_t _empty;
  uint32_t _player;
  uint32_t _entity;

  simde__m128i _vsolid;
  simde__m128i _vpassable;
  simde__m128i _vzero;

  struct snapshot final {
    int32_t cx{std::numeric_limits<int32_t>::min()};
    int32_t cy{std::numeric_limits<int32_t>::min()};
    size_t count{};

    constexpr auto operator<=>(const snapshot&) const noexcept = default;
  };

  snapshot _previous{};

  std::unique_ptr<SDL_Texture, SDL_Deleter> _texture;
  std::vector<uint32_t> _pixels;
  std::vector<std::pair<float, float>> _positions;
  float _position_x{};
  float _position_y{};
};
