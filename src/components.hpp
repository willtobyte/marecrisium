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
    int name{LUA_NOREF};
  } identity;
  uint16_t offset{};
  uint8_t count{};
};

static_assert(std::is_trivially_copyable_v<clip>, "clip must be trivially copyable");

struct animation final {
  const spritesheet* sheet{};
  float elapsed{};
  uint8_t active{};
  uint8_t current{};
};

static_assert(std::is_trivially_copyable_v<animation>, "animation must be trivially copyable");

struct prototype final {
  int table{LUA_NOREF};
  int kind{LUA_NOREF};
  int on_loop{LUA_NOREF};
  int on_animation_end{LUA_NOREF};
  int on_animation_begin{LUA_NOREF};
  int on_spawn{LUA_NOREF};
  int on_press{LUA_NOREF};
  int on_release{LUA_NOREF};
  int on_hover{LUA_NOREF};
  int on_unhover{LUA_NOREF};
};

static_assert(std::is_trivially_copyable_v<prototype>, "prototype must be trivially copyable");

struct scriptable final {
  static constexpr auto in_place_delete = true;

  const prototype* blueprint{};
  int handle{LUA_NOREF};
  int label{LUA_NOREF};
};

static_assert(std::is_trivially_copyable_v<scriptable>, "scriptable must be trivially copyable");

struct bounds final {
  float x{};
  float y{};
  float width{};
  float height{};
};

[[nodiscard]] constexpr bounds bounds_of(const frame& current, const transform& tf) {
  return {
    tf.x + current.collider.offset_x * tf.scale,
    tf.y + current.collider.offset_y * tf.scale,
    current.collider.width * tf.scale,
    current.collider.height * tf.scale,
  };
}

struct renderable final {
  int z{};
};

static_assert(std::is_trivially_copyable_v<renderable>, "renderable must be trivially copyable");

struct reorder final {
  bool dirty{true};
};

static_assert(std::is_trivially_copyable_v<reorder>, "reorder must be trivially copyable");
