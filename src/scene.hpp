#pragma once

class scene final {
public:
  explicit scene(std::string_view key);
  ~scene();

  void on_enter();

  void update(float delta);

  void draw();

  void on_leave();

private:
  static constexpr auto none = std::numeric_limits<uint32_t>::max();

  std::vector<object> _objects{};
  std::vector<uint32_t> _order{};
  std::vector<uint32_t> _loops{};

  overlay _overlay;

  std::unique_ptr<pixmap> _background{};

  friend class director;

  int _table{LUA_NOREF};
  int _pool{LUA_NOREF};

  int _on_enter{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_leave{LUA_NOREF};

  uint32_t _hovered{none};
  uint32_t _mouse_previous_buttons{};

  dirty _dirty{};
  scheduler _scheduler{};
  playback _playback{};
};
