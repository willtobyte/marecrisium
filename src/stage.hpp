#pragma once

class stringpool;
class tilemap;

class stage final {
public:
  explicit stage(std::string_view name);
  ~stage();

  void on_enter();

  void on_leave();

  void on_tick(uint64_t tick);

  void update(float delta);

  void draw() const;

  [[nodiscard]] int raycast(lua_State* state, entt::entity caller, float x, float y, float angle, float distance);

private:
  std::string _name;
  entt::registry _registry;
  std::unique_ptr<stringpool> _stringpool;
  std::vector<sound*> _sounds;
  b2WorldId _world;
  float _accumulator = .0f;
  uint32_t _mouse_previous_buttons{};
  std::vector<entt::entity> _hovering;
  std::vector<entt::entity> _hits;
  std::unique_ptr<tilemap> _tilemap;
  int _reference = LUA_NOREF;
  int _environment_reference = LUA_NOREF;
  int _pool_reference = LUA_NOREF;

  void dispatch_click(float x, float y, const char* button);

  void dispatch_hover(float x, float y);

  void dispatch_unhover();

  void dispatch_collision(entt::entity entity, entt::entity other_entity, const char* callback_name, const b2Vec2* normal = nullptr);

  void dispatch_screen_event(const objectproxy& proxy, const char* callback_name, std::string_view direction);
};
