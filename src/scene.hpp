#pragma once

class scene final {
public:
  explicit scene(std::string_view name);
  ~scene();

  void on_enter();

  void update(float delta);

  void draw();

  void on_leave();

private:
  std::vector<object> _objects{};
  std::vector<uint32_t> _order{};
  std::vector<uint32_t> _loops{};
  std::vector<std::unique_ptr<sound>> _sounds{};

  overlay _overlay;

  std::unique_ptr<pixmap> _background{};

  friend class director;

  int _table{LUA_NOREF};
  int _pool{LUA_NOREF};

  int _on_enter{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_leave{LUA_NOREF};

  timer _timer{};

  bool _dirty{true};
};
