#pragma once

class particle;

class particlesystem final {
public:
  particlesystem() = default;
  ~particlesystem() = default;

  [[nodiscard]] particle& add(std::string_view name, std::string_view kind, float x, float y, bool active);

  void update(float delta) noexcept;
  void draw(float camera_x, float camera_y) const noexcept;

  void clear();

private:
  std::unordered_map<entt::id_type, particle> _particles;
};
