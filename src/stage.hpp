#pragma once

class stage final {
public:
  explicit stage(std::string_view name);
  ~stage();

  void on_enter();

  void on_leave();

  void update(float delta);

  void draw() const;

private:
  static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;
  static constexpr int WORLD_SUBSTEPS = 4;

  std::string _name;
  entt::registry _registry;
  pixmappool _pixmaps;
  b2WorldId _world;
  float _accumulator = .0f;
  int _reference = LUA_NOREF;
  int _environment_reference = LUA_NOREF;
  int _pool_reference = LUA_NOREF;
};
