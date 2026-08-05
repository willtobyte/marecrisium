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
  float scale{1.f};
  float angle{};
  float alpha{255.f};
  bool shown{true};
  mirror flip{mirror::none};
};

static_assert(std::is_trivially_copyable_v<transform>, "transform must be trivially copyable");
static_assert(sizeof(transform) == 24, "transform must fit in 24 bytes");

struct frame final {
  float u0{};
  float v0{};
  float u1{};
  float v1{};
  float width{};
  float height{};
  struct {
    float offset_x{};
    float offset_y{};
    float width{};
    float height{};
  } collider;
  float duration{};
};

static_assert(std::is_trivially_copyable_v<frame>, "frame must be trivially copyable");
static_assert(sizeof(frame) == 44, "frame must fit in 44 bytes");

struct clip final {
  struct {
    entt::id_type hash{};
    int name_ref{LUA_NOREF};
  } identity;
  uint16_t offset{};
  uint8_t count{};
  sound* effect{};
};

static_assert(std::is_trivially_copyable_v<clip>, "clip must be trivially copyable");

struct animation final {
  const spritesheet* sheet{};
  float elapsed{};
  uint8_t active{};
  uint8_t current{};
  bool playing{};
};

static_assert(std::is_trivially_copyable_v<animation>, "animation must be trivially copyable");

struct prototype final {
  int table_ref{LUA_NOREF};
  int kind_ref{LUA_NOREF};
  int on_loop_ref{LUA_NOREF};
  int on_animation_end_ref{LUA_NOREF};
  int on_animation_begin_ref{LUA_NOREF};
  int on_spawn_ref{LUA_NOREF};
  int on_press_ref{LUA_NOREF};
  int on_release_ref{LUA_NOREF};
  int on_hover_ref{LUA_NOREF};
  int on_unhover_ref{LUA_NOREF};
};

static_assert(std::is_trivially_copyable_v<prototype>, "prototype must be trivially copyable");

struct scriptable final {
  static constexpr auto in_place_delete = true;

  const prototype* blueprint{};
  entt::id_type name{};
  entt::id_type kind{};
  int handle_ref{LUA_NOREF};
  int label_ref{LUA_NOREF};
};

static_assert(std::is_trivially_copyable_v<scriptable>, "scriptable must be trivially copyable");

struct body final {
  static constexpr auto in_place_delete = true;

  b2BodyId id{b2_nullBodyId};
  b2ShapeId shape{b2_nullShapeId};
  float extent_x{};
  float extent_y{};
  float origin_x{};
  float origin_y{};
  bool dirty{true};
};

static_assert(std::is_trivially_copyable_v<body>, "body must be trivially copyable");

[[nodiscard]] constexpr b2Vec2 center_of(const body& b, const transform& tf) {
  return {tf.x + b.origin_x + b.extent_x, tf.y + b.origin_y + b.extent_y};
}

struct renderable final {
  int z{};
};

static_assert(std::is_trivially_copyable_v<renderable>, "renderable must be trivially copyable");

struct reorder final {
  bool dirty{true};
};

static_assert(std::is_trivially_copyable_v<reorder>, "reorder must be trivially copyable");
