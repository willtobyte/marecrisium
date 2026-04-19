#pragma once

struct blob final {
  std::unique_ptr<uint8_t[]> ptr;
  std::size_t length{};

  const uint8_t* data() const { return ptr.get(); }
  std::size_t size() const { return length; }
  operator std::span<const uint8_t>() const { return {ptr.get(), length}; }
};

class io final {
public:
  io() = delete;
  ~io() = delete;

  static bool exists(std::string_view filename);

  static blob read(std::string_view filename);

  static std::vector<std::string> enumerate(std::string_view directory);
};
