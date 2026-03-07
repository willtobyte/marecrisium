#pragma once

class pixmap;

struct transform final {
  float x{};
  float y{};
  float scale{1.0f};
  float angle{};
  float alpha{255.0f};
  bool shown{true};
};

static_assert(std::is_trivially_copyable_v<transform>);

struct frame final {
  float x{};
  float y{};
  float w{};
  float h{};
  float duration{};
  float cx{};
  float cy{};
  float cw{};
  float ch{};
  bool collidable{};
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

enum class body_type : uint8_t {
  kinematic,
  dynamic,
  fixed,
};

struct body final {
  b2BodyId id{b2_nullBodyId};
  b2ShapeId shape{b2_nullShapeId};
  float cached_hx{};
  float cached_hy{};
  body_type type{body_type::kinematic};
};

static_assert(std::is_trivially_copyable_v<body>);

struct boundary final {
  uint8_t previous{};

  static constexpr uint8_t left = 1 << 0;
  static constexpr uint8_t right = 1 << 1;
  static constexpr uint8_t top = 1 << 2;
  static constexpr uint8_t bottom = 1 << 3;
};

static_assert(std::is_trivially_copyable_v<boundary>);
