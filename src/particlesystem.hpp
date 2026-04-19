#pragma once

class particle;

class particlesystem final {
public:
  particlesystem() = default;
  ~particlesystem() = default;

  [[nodiscard]] particle* add(std::string_view name, std::string_view kind, float x, float y, bool active);

  void update(float delta);
  void draw();

  void clear();

private:
  ankerl::unordered_dense::map<entt::id_type, std::unique_ptr<particle>> _particles;
};
