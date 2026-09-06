#pragma once

class io final {
public:
  io() = delete;
  ~io() = delete;

  static void mount(std::string_view filename);
  static std::optional<std::span<const uint8_t>> try_read(std::string_view filename);
  static std::span<const uint8_t> read(std::string_view filename);
};
