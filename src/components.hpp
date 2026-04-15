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
  float u0{};
  float v0{};
  float u1{};
  float v1{};
  bool collidable{};
};

static_assert(std::is_trivially_copyable_v<frame>);

struct clip final {
  struct {
    entt::id_type hash{};
    int reference{LUA_NOREF};
  } identity;
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
  static constexpr auto in_place_delete = true;

  b2BodyId id{b2_nullBodyId};
  b2ShapeId shape{b2_nullShapeId};
  float extent_x{};
  float extent_y{};
  body_type type{body_type::kinematic};
};

static_assert(std::is_trivially_copyable_v<body>);

[[nodiscard]] inline bool alive(const body& b) noexcept {
  return b2Body_IsValid(b.id);
}

[[nodiscard]] inline bool propelled(const body& b) noexcept {
  return b.type == body_type::dynamic && b2Body_IsValid(b.id);
}

[[nodiscard]] inline bool anchored(const body& b) noexcept {
  return b.type != body_type::kinematic && b2Body_IsValid(b.id);
}

[[nodiscard]] inline b2Vec2 center_of(const body& b, const transform& tf, const frame* fr = nullptr) noexcept {
  const auto ox = fr ? fr->bound_x : .0f;
  const auto oy = fr ? fr->bound_y : .0f;
  return {tf.x + ox + b.extent_x, tf.y + oy + b.extent_y};
}

struct boundary final {
  static constexpr auto in_place_delete = true;

  uint8_t previous{};

  static constexpr uint8_t left = 1 << 0;
  static constexpr uint8_t right = 1 << 1;
  static constexpr uint8_t top = 1 << 2;
  static constexpr uint8_t bottom = 1 << 3;
};

static_assert(std::is_trivially_copyable_v<boundary>);

struct sleepable final {
  static constexpr auto in_place_delete = true;
};

static_assert(std::is_trivially_copyable_v<sleepable>);

struct dormant final {
  static constexpr auto in_place_delete = true;
};

static_assert(std::is_trivially_copyable_v<dormant>);


struct renderable final {
  int z{};
};

static_assert(std::is_trivially_copyable_v<renderable>);

struct reorder final {
  bool dirty{true};
};

static_assert(std::is_trivially_copyable_v<reorder>);
