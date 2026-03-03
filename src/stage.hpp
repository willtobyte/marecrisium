#pragma once

class stage final {
public:
  explicit stage(std::string_view name);
  ~stage();

  void update(float delta);

  void draw() const;

private:
  static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;
  static constexpr int WORLD_SUBSTEPS = 4;

  entt::registry _registry;
  b2WorldId _world;
  float _accumulator = .0f;
};
