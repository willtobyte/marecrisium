#pragma once

class pixmap;

struct spritesheet final {
  const pixmap* pixmap{};
  const sequence* sequences{};
  const frame* frames{};
  struct {
    uint16_t width{};
    uint16_t height{};
  } source;
  uint8_t count{};
  uint8_t initial{};
  const std::unordered_map<std::string_view, uint8_t, transparent_string_hash, std::equal_to<>>* lookup{};
};
