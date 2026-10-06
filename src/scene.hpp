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
  static bool on_event(void* userdata, SDL_Event* event);

  std::vector<object> _objects{};
  std::vector<uint32_t> _order{};
  std::vector<uint32_t> _loops{};

  const object* _hover{};

  overlay _overlay;

  pixmap _background;

  friend class director;

  int _table{LUA_NOREF};
  int _pool{LUA_NOREF};

  int _on_enter{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_leave{LUA_NOREF};

  struct {
    float x{};
    float y{};
    bool pressed{};
  } _pointer;

  dirty _dirty{};
  scheduler _scheduler{};
  playback _playback{};
};
