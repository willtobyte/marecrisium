#pragma once

class io final {
public:
  io() = delete;
  ~io() = delete;

  static bool exists(std::string_view filename) noexcept;

  static std::vector<uint8_t> read(std::string_view filename);

  static std::vector<std::string> enumerate(std::string_view directory);
};
