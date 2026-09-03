#pragma once

class bytes final {
public:
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

  bytes(const uint8_t* data, size_t size)
      : _data(data), _size(size) {}

  const uint8_t* _data{};
  size_t _size{};
};

class io final {
public:
  io() = delete;
  ~io() = delete;

  static void mount(std::string_view filename);
  static std::optional<bytes> try_read(std::string_view filename);
  static bytes read(std::string_view filename);
};
