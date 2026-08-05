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

  void expose();

  void conceal();

  void spawn(std::string_view name, std::string_view kind, float x, float y);

  [[nodiscard]] uint8_t pick_at(float x, float y, entt::entity* buffer, uint8_t capacity) const noexcept;

  [[nodiscard]] entt::entity find_topmost(std::span<const entt::entity> hits) const noexcept;

  void dispatch_press(float x, float y, const char* button);

  void dispatch_release(float x, float y, const char* button);

  void dispatch_hover(float x, float y);

  void dispatch_unhover();

  void dispatch_miss(int callback, float x, float y, const char* button);

private:
  entt::registry _registry{};
  std::string _name{};
  std::vector<std::string> _foregrounds{};

  std::vector<sound*> _sounds{};

  b2WorldId _world{};

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
