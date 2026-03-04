#pragma once

class pixmappool;
class sourcepool;
class stringpool;

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
  std::unique_ptr<pixmappool> _pixmaps;
  std::unique_ptr<sourcepool> _sources;
  std::unique_ptr<stringpool> _strings;
  b2WorldId _world;
  float _accumulator = .0f;
  int _reference = LUA_NOREF;
  int _environment_reference = LUA_NOREF;
  int _pool_reference = LUA_NOREF;

  void dispatch_collision(entt::entity entity, entt::entity other_entity, const char* callback_name);

  void dispatch_screen_event(const objectproxy& proxy, const char* callback_name, std::string_view direction);
};
