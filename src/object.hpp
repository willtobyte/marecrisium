#pragma once

struct spritesheet;

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

struct clip final {
  uint16_t offset{};
  uint8_t count{};
};

static_assert(std::is_trivially_copyable_v<clip>, "clip must be trivially copyable");

struct prototype final {
  int table{LUA_NOREF};
  int kind{LUA_NOREF};
  int on_loop{LUA_NOREF};
  int on_spawn{LUA_NOREF};
};

static_assert(std::is_trivially_copyable_v<prototype>, "prototype must be trivially copyable");

struct object final {
  struct sprite final {
    const spritesheet* sheet{};
    float x{};
    float y{};
    float scale{1.f};
    float angle{};
    float alpha{255.f};
    int z{};
    bool shown{true};
    mirror::value mirror{mirror::value::none};
  } sprite;

  struct script final {
    const prototype* blueprint{};
    int instance{LUA_NOREF};
    int label{LUA_NOREF};
  } script;

  struct motion final {
    float elapsed{};
    uint8_t active{};
    uint8_t current{};
  } motion;
};

static_assert(std::is_trivially_copyable_v<object>, "object must be trivially copyable");

struct proxy final {
  object* object{};
  bool* dirty{};
};

static_assert(std::is_trivially_copyable_v<proxy>, "proxy must be trivially copyable");

namespace objects {
  void wire();
  void bind(object& object, bool& dirty, std::string_view name, std::string_view kind);
}
