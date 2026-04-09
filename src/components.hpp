#pragma once

class pixmap;
class sound;
struct spritesheet;

enum class mirror : uint8_t {
  none = SDL_FLIP_NONE,
  horizontal = SDL_FLIP_HORIZONTAL,
  vertical = SDL_FLIP_VERTICAL,
  both = SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL,
};

struct transform final {
  float x{};
  float y{};
  float previous_x{};
  float previous_y{};
  float scale{1.f};
  float angle{};
  float alpha{255.f};
  bool shown{true};
  mirror flip{mirror::none};
};

static_assert(std::is_trivially_copyable_v<transform>);

struct frame final {
  float x{};
  float y{};
  float width{};
  float height{};
  float duration{};
  float bound_x{};
  float bound_y{};
  float bound_width{};
  float bound_height{};
  bool collidable{};
};

static_assert(std::is_trivially_copyable_v<frame>);

struct clip final {
  entt::id_type name{};
  uint16_t offset{};
  uint8_t count{};
  sound* effect{};
};

static_assert(std::is_trivially_copyable_v<clip>);

struct animation final {
  const spritesheet* sheet{};
  float elapsed{};
  uint8_t active{};
  uint8_t current{};
  bool playing{};
};

static_assert(std::is_trivially_copyable_v<animation>);

enum class body_type : uint8_t {
  kinematic,
  dynamic,
  stationary,
};

struct body final {
  b2BodyId id{b2_nullBodyId};
  b2ShapeId shape{b2_nullShapeId};
  float extent_x{};
  float extent_y{};
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

struct sleepable final {};

static_assert(std::is_trivially_copyable_v<sleepable>);

struct dormant final {};

static_assert(std::is_trivially_copyable_v<dormant>);


struct renderable final {
  int z{};
};

static_assert(std::is_trivially_copyable_v<renderable>);

struct reorder final {
  bool dirty{true};
};

static_assert(std::is_trivially_copyable_v<reorder>);
