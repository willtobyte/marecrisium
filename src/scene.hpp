#pragma once

class scene final {
public:
  explicit scene(std::string name);
  ~scene();

  void update(float delta);

  void draw();

  void on_enter();

  void on_leave();

private:
  entt::registry _registry{};
  systems _systems{_registry};
  std::string _name{};
  std::unique_ptr<pixmap> _background{};
  overlay _overlay;

  friend class director;

  int _table{LUA_NOREF};
  int _pool{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_camera{LUA_NOREF};
  int _on_enter{LUA_NOREF};
  int _on_leave{LUA_NOREF};
  int _on_press{LUA_NOREF};
  int _on_release{LUA_NOREF};

  entt::entity _hovered{entt::null};
  uint32_t _mouse_previous_buttons{};

  timer::group _timer{};
};
