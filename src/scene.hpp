#pragma once

class pixmap;

class scene final {
public:
  explicit scene(std::string name);
  ~scene();

  void update(float delta);

  void draw();

  void on_enter();

  void on_leave();

  [[nodiscard]] entt::entity pick(float x, float y) noexcept;

private:
  entt::registry _registry{};
  std::string _name{};
  std::vector<std::string> _foregrounds{};

  std::vector<sound*> _sounds{};

  friend class director;

  int _table_ref{LUA_NOREF};
  int _pool_ref{LUA_NOREF};
  int _on_loop_ref{LUA_NOREF};
  int _on_camera_ref{LUA_NOREF};
  int _on_enter_ref{LUA_NOREF};
  int _on_leave_ref{LUA_NOREF};
  int _on_press_ref{LUA_NOREF};
  int _on_release_ref{LUA_NOREF};

  entt::entity _hovered{entt::null};
  uint32_t _mouse_previous_buttons{};

  timer::group _timer{};
};
