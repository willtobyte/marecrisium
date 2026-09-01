#pragma once

class scheduler final {
public:
  using callback = void (*)(std::uint64_t);

  struct handle final {
    std::weak_ptr<void> life{};
    scheduler* owner{};
    std::uint32_t slot{};
    std::uint32_t generation{};
  };

  scheduler() = default;
  ~scheduler() noexcept;

  handle add(double milliseconds, bool repeat, callback call, callback release, std::uint64_t data);

  static void cancel(const handle& value) noexcept;
  static void pause(const handle& value) noexcept;
  static void resume(const handle& value) noexcept;
  static bool active(const handle& value) noexcept;

  void clear() noexcept;

private:
  static constexpr auto capacity = 16uz;

  struct entry final {
    callback call{};
    callback release{};
    std::uint64_t data{};
    double deadline{};
    double period{};
    std::uint32_t generation{};
  };

  static entry* find(const handle& value) noexcept;

  void update(float delta);

  void wire();
  void activate();
  void suspend() noexcept;

  std::array<entry, capacity> _entries{};
  std::shared_ptr<void> _life{std::make_shared<std::byte>()};
  double _now{};

  int _table{LUA_NOREF};
  std::uint16_t _used{};
  std::uint16_t _live{};
  std::uint16_t _hold{};
  std::uint16_t _repeat{};
  bool _active{};

  friend class scene;
};
