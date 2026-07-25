#pragma once

class bytes final {
public:
  bytes(bytes&& other) noexcept
      : _storage(std::move(other._storage)),
        _data(std::exchange(other._data, nullptr)),
        _size(std::exchange(other._size, 0uz)) {}

  bytes& operator=(bytes&& other) noexcept {
    _storage = std::move(other._storage);
    _data = std::exchange(other._data, nullptr);
    _size = std::exchange(other._size, 0uz);
    return *this;
  }

  [[nodiscard]] const uint8_t* data() const noexcept {
    return _data;
  }

  [[nodiscard]] size_t size() const noexcept {
    return _size;
  }

  operator std::span<const uint8_t>() const noexcept {
    return {data(), size()};
  }

private:
  friend class io;

  bytes() = default;

  explicit bytes(size_t size)
      : _storage(std::make_unique_for_overwrite<uint8_t[]>(size)),
        _data(_storage.get()), _size(size) {}

  bytes(const uint8_t* data, size_t size)
      : _data(data), _size(size) {}

  [[nodiscard]] uint8_t* writable() noexcept {
    return _storage.get();
  }

  std::unique_ptr<uint8_t[]> _storage;
  const uint8_t* _data{};
  size_t _size{};
};

class io final {
public:
  io() = delete;
  ~io() = delete;

  static void mount(std::string_view filename);
  static bool exists(std::string_view filename) noexcept;
  static bytes read(std::string_view filename);
};
