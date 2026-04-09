#pragma once

class pixmap;

struct spritesheet final {
  const pixmap* pixmap{};
  const clip* clips{};
  const frame* frames{};
  uint8_t count{};
  uint8_t initial{};
  bool collidable{};
};

static_assert(std::is_trivially_copyable_v<spritesheet>);
