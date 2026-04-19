#pragma once

struct blob final {
  std::unique_ptr<uint8_t[]> ptr;
  std::size_t length{};

  [[nodiscard]] const uint8_t* data() const { return ptr.get(); }
  [[nodiscard]] std::size_t size() const { return length; }
  [[nodiscard]] operator std::span<const uint8_t>() const { return {ptr.get(), length}; }
};

class io final {
public:
  io() = delete;
  ~io() = delete;

  [[nodiscard]] static bool exists(std::string_view filename);

  [[nodiscard]] static blob read(std::string_view filename);

  [[nodiscard]] static std::vector<std::string> enumerate(std::string_view directory);
};
