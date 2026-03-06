#pragma once

class pixmappool;
class soundpool;
class sourcepool;
class stringpool;
struct particlebatch;

class stage final {
public:
  stage(std::string_view name, pixmappool& pixmaps, soundpool& sounds, sourcepool& sources);
  ~stage();

  void on_enter();

  void on_leave();

  void update(float delta);

  void draw() const;

  int raycast(lua_State* state, entt::entity caller, float x, float y, float angle, float distance);

private:
  static constexpr float FIXED_TIMESTEP = 1.f / 60.f;
  static constexpr int WORLD_SUBSTEPS = 4;

  std::string _name;
  entt::registry _registry;
  pixmappool& _pixmappool;
  soundpool& _soundpool;
  sourcepool& _sourcepool;
  std::unique_ptr<stringpool> _stringpool;
  std::vector<sound*> _sounds;
  std::vector<std::unique_ptr<particlebatch>> _particles;
  b2WorldId _world;
  float _accumulator = .0f;
  uint32_t _mouse_previous_buttons{};
  int _reference = LUA_NOREF;
  int _environment_reference = LUA_NOREF;
  int _pool_reference = LUA_NOREF;

  void dispatch_click(float x, float y, const char* button);

  void dispatch_collision(entt::entity entity, entt::entity other_entity, const char* callback_name);

  void dispatch_screen_event(const objectproxy& proxy, const char* callback_name, std::string_view direction);
};
