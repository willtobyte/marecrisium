#pragma once

class timer final {
public:
  using callback = void (*)(std::uint64_t);

  struct handle final {
    std::weak_ptr<void> life{};
    std::uint32_t slot{};
    std::uint32_t generation{};
  };

  timer();
  ~timer() noexcept;

  handle add(double milliseconds, bool repeat, callback call, callback release, std::uint64_t data);

  static void cancel(const handle& value) noexcept;
  static void pause(const handle& value) noexcept;
  static void resume(const handle& value) noexcept;
  static bool active(const handle& value) noexcept;

  void clear() noexcept;
  void update(float delta);

private:
  struct record;
  struct state;

  void wire();

  static record* find(state& current, const handle& value) noexcept;
  static void deactivate(state& current, record& node, bool release) noexcept;

  std::shared_ptr<state> _state;
  int _table{LUA_NOREF};

  friend class scene;
};
