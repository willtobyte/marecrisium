#include "gamepad.hpp"

static constexpr float DEADZONE_THRESHOLD = 0.1f;

static float deadzone(Sint16 raw) {
  const auto value = static_cast<float>(raw) / 32767.0f;
  if (std::abs(value) < DEADZONE_THRESHOLD)
    return .0f;

  return value;
}

static std::unique_ptr<SDL_Gamepad, SDL_Deleter> ptr{nullptr};

static bool on_event(void *, SDL_Event *event) {
  switch (event->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
      if (!ptr)
        ptr.reset(SDL_OpenGamepad(event->gdevice.which));
      break;

    case SDL_EVENT_GAMEPAD_REMOVED:
      if (ptr && SDL_GetGamepadID(ptr.get()) == event->gdevice.which)
        ptr.reset();
      break;

    default:
      break;
  }

  return true;
}

static int gamepad_rumble(lua_State *state) {
  const auto low = std::clamp(static_cast<float>(luaL_checknumber(state, 2)), .0f, 1.0f);
  const auto high = std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 1.0f);
  const auto duration = static_cast<uint32_t>(luaL_checkinteger(state, 4));

  const auto low16 = static_cast<uint16_t>(low * 65535.0f);
  const auto high16 = static_cast<uint16_t>(high * 65535.0f);

  if (!ptr) [[unlikely]]
    return lua_pushboolean(state, false), 1;

  return lua_pushboolean(state, SDL_RumbleGamepad(ptr.get(), low16, high16, duration)), 1;
}

static int push_axis(lua_State *state, SDL_GamepadAxis a) {
  if (ptr) [[likely]]
    return lua_pushnumber(state, static_cast<double>(deadzone(SDL_GetGamepadAxis(ptr.get(), a)))), 1;
  return lua_pushnumber(state, 0), 1;
}

static int push_button(lua_State *state, SDL_GamepadButton b) {
  if (ptr) [[likely]]
    return lua_pushboolean(state, SDL_GetGamepadButton(ptr.get(), b)), 1;
  return lua_pushboolean(state, false), 1;
}

static int gamepad_index(lua_State *state) {
  const std::string_view name = luaL_checkstring(state, 2);

  if (name == "connected")      return lua_pushboolean(state, ptr != nullptr), 1;
  if (name == "rumble")          return lua_pushcfunction(state, gamepad_rumble), 1;

  if (name == "name") {
    if (ptr) [[likely]]
      return lua_pushstring(state, SDL_GetGamepadName(ptr.get())), 1;
    return lua_pushstring(state, ""), 1;
  }

  if (name == "left_x")         return push_axis(state, SDL_GAMEPAD_AXIS_LEFTX);
  if (name == "left_y")         return push_axis(state, SDL_GAMEPAD_AXIS_LEFTY);
  if (name == "right_x")        return push_axis(state, SDL_GAMEPAD_AXIS_RIGHTX);
  if (name == "right_y")        return push_axis(state, SDL_GAMEPAD_AXIS_RIGHTY);
  if (name == "trigger_left")   return push_axis(state, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
  if (name == "trigger_right")  return push_axis(state, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

  if (name == "south")          return push_button(state, SDL_GAMEPAD_BUTTON_SOUTH);
  if (name == "east")           return push_button(state, SDL_GAMEPAD_BUTTON_EAST);
  if (name == "west")           return push_button(state, SDL_GAMEPAD_BUTTON_WEST);
  if (name == "north")          return push_button(state, SDL_GAMEPAD_BUTTON_NORTH);
  if (name == "back")           return push_button(state, SDL_GAMEPAD_BUTTON_BACK);
  if (name == "guide")          return push_button(state, SDL_GAMEPAD_BUTTON_GUIDE);
  if (name == "start")          return push_button(state, SDL_GAMEPAD_BUTTON_START);
  if (name == "shoulder_left")  return push_button(state, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
  if (name == "shoulder_right") return push_button(state, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
  if (name == "stick_left")     return push_button(state, SDL_GAMEPAD_BUTTON_LEFT_STICK);
  if (name == "stick_right")    return push_button(state, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
  if (name == "up")             return push_button(state, SDL_GAMEPAD_BUTTON_DPAD_UP);
  if (name == "down")           return push_button(state, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
  if (name == "left")           return push_button(state, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
  if (name == "right")          return push_button(state, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);

  return lua_pushnil(state), 1;
}

void gamepad::wire() {
  SDL_AddEventWatch(on_event, nullptr);

  auto count = 0;
  const auto ids = std::unique_ptr<SDL_JoystickID[], SDL_Deleter>(SDL_GetGamepads(&count));
  if (ids && count > 0) {
    ptr.reset(SDL_OpenGamepad(ids[0]));
  }

  lua_newuserdata(L, 1);

  luaL_newmetatable(L, "Gamepad");
  lua_pushcfunction(L, gamepad_index);
  lua_setfield(L, -2, "__index");

  lua_setmetatable(L, -2);
  lua_setglobal(L, "gamepad");
}
