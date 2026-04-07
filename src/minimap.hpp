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

  color _solid;
  color _passable;
  color _empty;
  color _player;
  color _entity;

  std::unique_ptr<SDL_Texture, SDL_Deleter> _texture;
  std::vector<uint32_t> _pixels;
  float _position_x{};
  float _position_y{};

  void rebuild() noexcept;
};
