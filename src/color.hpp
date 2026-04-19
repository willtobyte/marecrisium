#pragma once

struct color final {
  uint8_t r{};
  uint8_t g{};
  uint8_t b{};

  explicit operator uint32_t() const {
    return (255u << 24) | (static_cast<uint32_t>(b) << 16)
         | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
  }
};

static_assert(std::is_trivially_copyable_v<color>);
