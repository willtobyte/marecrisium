#pragma once

struct backing final {
  const std::size_t length;
  uint32_t count;

  uint8_t *data() noexcept { return reinterpret_cast<uint8_t *>(this + 1); }
  const uint8_t *data() const noexcept { return reinterpret_cast<const uint8_t *>(this + 1); }
};

struct releaser {
  void operator()(backing *storage) const noexcept {
    if (--storage->count != 0)
      return;

    std::destroy_at(storage);
    ::operator delete(storage);
  }
};

struct blob final {
private:
  std::unique_ptr<backing, releaser> storage;

  blob() noexcept = default;
  explicit blob(std::unique_ptr<backing, releaser> storage) noexcept;

  friend class capture;

public:
  blob(blob &&other) noexcept = default;
  blob &operator=(blob &&other) noexcept = default;

  blob(const blob &) = delete;
  blob &operator=(const blob &) = delete;

  const uint8_t *data() const noexcept;
  std::size_t size() const noexcept;
  operator std::span<const uint8_t>() const noexcept;
};

static_assert(sizeof(blob) == sizeof(std::unique_ptr<backing, releaser>));

class capture final {
  blob content;

public:
  capture() noexcept;
  ~capture();

  capture(const capture &) = delete;
  capture &operator=(const capture &) = delete;

  blob finish() noexcept;
  void store(backing *storage) noexcept;
};

class io final {
public:
  io() = delete;
  ~io() = delete;

  static bool exists(std::string_view filename);

  static blob read(std::string_view filename);

  static std::vector<std::string> enumerate(std::string_view directory);
};
