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
    struct {
      float x{};
      float y{};
    } offset;
    float width{};
    float height{};
  } collider;
  float duration{};
  struct {
    uint16_t x{};
    uint16_t y{};
  } offset;
};

static_assert(std::is_trivially_copyable_v<frame>, "frame must be trivially copyable");

struct sequence final {
  int name{LUA_NOREF};
  uint16_t offset{};
  uint8_t count{};
  bool loop{true};
};

static_assert(std::is_trivially_copyable_v<sequence>, "sequence must be trivially copyable");

struct prototype final {
  int table{LUA_NOREF};
  int kind{LUA_NOREF};
  int on_loop{LUA_NOREF};
  int on_spawn{LUA_NOREF};
  int on_hover{LUA_NOREF};
  int on_unhover{LUA_NOREF};
  int on_click{LUA_NOREF};
};

static_assert(std::is_trivially_copyable_v<prototype>, "prototype must be trivially copyable");

struct object final {
  struct sprite final {
    const spritesheet* sheet{};
    float x{};
    float y{};
    float scale{1.f};

    struct bounds final {
      float x{};
      float y{};
      float width{};
      float height{};
    } bounds;

    float angle{};
    int z{};
    uint8_t alpha{255};
    bool shown{true};
    mirror::value mirror{mirror::value::none};

    void resize(const float width, const float height, const float value) {
      scale = value;
      bounds.width = width * value;
      bounds.height = height * value;
      bounds.x = (width - bounds.width) * .5f;
      bounds.y = (height - bounds.height) * .5f;
    }
  } sprite;

  struct script final {
    const prototype* blueprint{};
    int instance{LUA_NOREF};
    int label{LUA_NOREF};
    int on_end{LUA_NOREF};
  } script;

  struct motion final {
    float elapsed{};
    uint8_t active{};
    uint8_t current{};
    bool ending{};
  } motion;
};

static_assert(std::is_trivially_copyable_v<object>, "object must be trivially copyable");

struct dirty final {
  bool order{true};
};

static_assert(std::is_trivially_copyable_v<dirty>, "dirty must be trivially copyable");

struct proxy final {
  object* object{};
  dirty* dirty{};
};

static_assert(std::is_trivially_copyable_v<proxy>, "proxy must be trivially copyable");

namespace objects {
  void wire();
  void bind(object& object, dirty& dirty, std::string_view name, std::string_view kind);
}
