#include "gamepad.hpp"

namespace {
  namespace property {
    constexpr auto connected = "connected"_hs;
    constexpr auto rumble = "rumble"_hs;
    constexpr auto led = "led"_hs;
    constexpr auto name = "name"_hs;
  }

  static const ankerl::unordered_dense::map<entt::id_type, SDL_GamepadAxis> axes{
    {"left_x"_hs, SDL_GAMEPAD_AXIS_LEFTX},
    {"left_y"_hs, SDL_GAMEPAD_AXIS_LEFTY},
    {"right_x"_hs, SDL_GAMEPAD_AXIS_RIGHTX},
    {"right_y"_hs, SDL_GAMEPAD_AXIS_RIGHTY},
    {"trigger_left"_hs, SDL_GAMEPAD_AXIS_LEFT_TRIGGER},
    {"trigger_right"_hs, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER},
  };

  static const ankerl::unordered_dense::map<entt::id_type, SDL_GamepadButton> buttons{
    {"south"_hs, SDL_GAMEPAD_BUTTON_SOUTH},
    {"east"_hs, SDL_GAMEPAD_BUTTON_EAST},
    {"west"_hs, SDL_GAMEPAD_BUTTON_WEST},
    {"north"_hs, SDL_GAMEPAD_BUTTON_NORTH},
    {"back"_hs, SDL_GAMEPAD_BUTTON_BACK},
    {"guide"_hs, SDL_GAMEPAD_BUTTON_GUIDE},
    {"start"_hs, SDL_GAMEPAD_BUTTON_START},
    {"shoulder_left"_hs, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"shoulder_right"_hs, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"stick_left"_hs, SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {"stick_right"_hs, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {"up"_hs, SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"down"_hs, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"left"_hs, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"right"_hs, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
  };
}

static constexpr float DEADZONE_THRESHOLD = .1f;

[[nodiscard]] static float deadzone(Sint16 axis) noexcept {
  const auto normalized = static_cast<float>(axis) / 32768.f;
  const auto magnitude = std::abs(normalized);
  if (magnitude < DEADZONE_THRESHOLD) [[likely]]
    return .0f;

  const auto sign = std::copysign(1.f, normalized);
  return sign * (magnitude - DEADZONE_THRESHOLD) / (1.f - DEADZONE_THRESHOLD);
}

static std::unique_ptr<SDL_Gamepad, SDL_Deleter> ptr{nullptr};

static bool on_event(void *, SDL_Event *event) {
  switch (event->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
      if (!ptr) [[unlikely]]
        ptr.reset(SDL_OpenGamepad(event->gdevice.which));
      break;

    case SDL_EVENT_GAMEPAD_REMOVED:
      if (ptr && SDL_GetGamepadID(ptr.get()) == event->gdevice.which) [[likely]]
        ptr.reset();
      break;

    default:
      break;
  }

  return true;
}

static int gamepad_rumble(lua_State *state) {
  const auto low = std::clamp(static_cast<float>(luaL_checknumber(state, 2)), .0f, 1.f);
  const auto high = std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 1.f);
  const auto duration = static_cast<uint32_t>(luaL_checkinteger(state, 4));

  const auto low16 = static_cast<uint16_t>(low * 65535.f);
  const auto high16 = static_cast<uint16_t>(high * 65535.f);

  if (!ptr) [[unlikely]] {
    lua_pushboolean(state, 0);
    return 1;
  }

  lua_pushboolean(state, static_cast<bool>(SDL_RumbleGamepad(ptr.get(), low16, high16, duration)) ? 1 : 0);
  return 1;
}

static int gamepad_led(lua_State *state) {
  const auto r = static_cast<uint8_t>(std::clamp(static_cast<float>(luaL_checknumber(state, 2)), .0f, 1.f) * 255.f);
  const auto g = static_cast<uint8_t>(std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 1.f) * 255.f);
  const auto b = static_cast<uint8_t>(std::clamp(static_cast<float>(luaL_checknumber(state, 4)), .0f, 1.f) * 255.f);

  if (!ptr) [[unlikely]] {
    lua_pushboolean(state, 0);
    return 1;
  }

  lua_pushboolean(state, static_cast<bool>(SDL_SetGamepadLED(ptr.get(), r, g, b)) ? 1 : 0);
  return 1;
}

static int push_gamepad_axis(lua_State *state, SDL_GamepadAxis axis) {
  if (!ptr) [[unlikely]]
    return lua_pushnumber(state, .0), 1;

  lua_pushnumber(state, static_cast<lua_Number>(deadzone(SDL_GetGamepadAxis(ptr.get(), axis))));
  return 1;
}

static int push_gamepad_button(lua_State *state, SDL_GamepadButton button) {
  if (!ptr) [[unlikely]]
    return lua_pushboolean(state, 0), 1;

  lua_pushboolean(state, static_cast<bool>(SDL_GetGamepadButton(ptr.get(), button)) ? 1 : 0);
  return 1;
}

static int gamepad_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (const auto it = axes.find(id); it != axes.end()) [[likely]]
    return push_gamepad_axis(state, it->second);

  if (const auto it = buttons.find(id); it != buttons.end()) [[likely]]
    return push_gamepad_button(state, it->second);

  switch (id) {
    case property::connected:
      lua_pushboolean(state, ptr != nullptr ? 1 : 0);
      return 1;

    case property::rumble:
      lua_pushcfunction(state, gamepad_rumble);
      return 1;

    case property::led:
      lua_pushcfunction(state, gamepad_led);
      return 1;

    case property::name:
      lua_pushstring(state, ptr ? SDL_GetGamepadName(ptr.get()) : "");
      return 1;

    default:
      lua_pushnil(state);
      return 1;
  }
}

void gamepad::wire() {
  SDL_AddEventWatch(on_event, nullptr);

  auto count = 0;
  const auto ids = std::unique_ptr<SDL_JoystickID[], SDL_Deleter>{SDL_GetGamepads(&count)};
  if (ids && count > 0) [[likely]] {
    ptr.reset(SDL_OpenGamepad(ids[0]));
  }

  metatable(L, "Gamepad", gamepad_index);

  singleton(L, "Gamepad", "gamepad");
}
