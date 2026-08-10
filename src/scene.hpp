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
  static constexpr auto none = std::numeric_limits<uint32_t>::max();

  std::string _name{};
  std::unique_ptr<pixmap> _background{};
  overlay _overlay;
  std::vector<sound*> _sounds{};
  std::vector<object> _objects{};
  std::vector<uint32_t> _order{};
  std::vector<uint32_t> _loops{};

  friend class director;

  int _table{LUA_NOREF};
  int _pool{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_camera{LUA_NOREF};
  int _on_enter{LUA_NOREF};
  int _on_leave{LUA_NOREF};
  int _on_press{LUA_NOREF};
  int _on_release{LUA_NOREF};

  uint32_t _hovered{none};
  uint32_t _mouse_previous_buttons{};
  bool _dirty{true};

  timer::group _timer{};
};
