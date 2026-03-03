#pragma once

class pixmap;

struct transform final {
  float x{};
  float y{};
  float scale{1.0f};
  float angle{};
  uint8_t alpha{255};
  bool shown{true};
};

static_assert(std::is_trivially_copyable_v<transform>);

struct frame final {
  float x{};
  float y{};
  float w{};
  float h{};
  float duration{};
};

static_assert(std::is_trivially_copyable_v<frame>);

struct clip final {
  entt::id_type name{};
  std::array<frame, 16> frames{};
  uint8_t count{};
};

static_assert(std::is_trivially_copyable_v<clip>);

struct animation final {
  const pixmap* pixmap{};
  std::array<clip, 8> clips{};
  uint8_t clip_count{};
  uint8_t active{};
  uint8_t current{};
  float elapsed{};
  bool playing{};
};

static_assert(std::is_trivially_copyable_v<animation>);
