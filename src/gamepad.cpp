#include "gamepad.hpp"

static constexpr float DEADZONE_THRESHOLD = .1f;

[[nodiscard]] static float deadzone(Sint16 raw) noexcept {
  const auto value = static_cast<float>(raw) / 32767.f;
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
  const auto low = std::clamp(static_cast<float>(luaL_checknumber(state, 2)), .0f, 1.f);
  const auto high = std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 1.f);
  const auto duration = static_cast<uint32_t>(luaL_checkinteger(state, 4));

  const auto low16 = static_cast<uint16_t>(low * 65535.f);
  const auto high16 = static_cast<uint16_t>(high * 65535.f);

  if (!ptr) [[unlikely]]
    return push(state, false);

  return push(state, static_cast<bool>(SDL_RumbleGamepad(ptr.get(), low16, high16, duration)));
}

static int push_gamepad_axis(lua_State *state, SDL_GamepadAxis a) {
  if (ptr) [[likely]]
    return push(state, deadzone(SDL_GetGamepadAxis(ptr.get(), a)));
  return push(state, .0f);
}

static int push_gamepad_button(lua_State *state, SDL_GamepadButton b) {
  if (ptr) [[likely]]
    return push(state, static_cast<bool>(SDL_GetGamepadButton(ptr.get(), b)));
  return push(state, false);
}

static int gamepad_index(lua_State *state) {
  enum class type : uint8_t { axis, button };

  struct entry final {
    type type;

    union {
      SDL_GamepadAxis axis;
      SDL_GamepadButton button;
    };
  };

  static const std::unordered_map<std::string_view, entry> mapping{
    {"left_x",         {type::axis,   {.axis = SDL_GAMEPAD_AXIS_LEFTX}}},
    {"left_y",         {type::axis,   {.axis = SDL_GAMEPAD_AXIS_LEFTY}}},
    {"right_x",        {type::axis,   {.axis = SDL_GAMEPAD_AXIS_RIGHTX}}},
    {"right_y",        {type::axis,   {.axis = SDL_GAMEPAD_AXIS_RIGHTY}}},
    {"trigger_left",   {type::axis,   {.axis = SDL_GAMEPAD_AXIS_LEFT_TRIGGER}}},
    {"trigger_right",  {type::axis,   {.axis = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER}}},
    {"south",          {type::button, {.button = SDL_GAMEPAD_BUTTON_SOUTH}}},
    {"east",           {type::button, {.button = SDL_GAMEPAD_BUTTON_EAST}}},
    {"west",           {type::button, {.button = SDL_GAMEPAD_BUTTON_WEST}}},
    {"north",          {type::button, {.button = SDL_GAMEPAD_BUTTON_NORTH}}},
    {"back",           {type::button, {.button = SDL_GAMEPAD_BUTTON_BACK}}},
    {"guide",          {type::button, {.button = SDL_GAMEPAD_BUTTON_GUIDE}}},
    {"start",          {type::button, {.button = SDL_GAMEPAD_BUTTON_START}}},
    {"shoulder_left",  {type::button, {.button = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER}}},
    {"shoulder_right", {type::button, {.button = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER}}},
    {"stick_left",     {type::button, {.button = SDL_GAMEPAD_BUTTON_LEFT_STICK}}},
    {"stick_right",    {type::button, {.button = SDL_GAMEPAD_BUTTON_RIGHT_STICK}}},
    {"up",             {type::button, {.button = SDL_GAMEPAD_BUTTON_DPAD_UP}}},
    {"down",           {type::button, {.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN}}},
    {"left",           {type::button, {.button = SDL_GAMEPAD_BUTTON_DPAD_LEFT}}},
    {"right",          {type::button, {.button = SDL_GAMEPAD_BUTTON_DPAD_RIGHT}}},
  };

  const std::string_view name = luaL_checkstring(state, 2);

  if (name == "connected")
    return push(state, ptr != nullptr);

  if (name == "rumble")
    return lua_pushcfunction(state, gamepad_rumble), 1;

  if (name == "name") {
    if (ptr) [[likely]]
      return push(state, SDL_GetGamepadName(ptr.get()));
    return push(state, "");
  }

  const auto it = mapping.find(name);
  if (it == mapping.end()) [[unlikely]]
    return lua_pushnil(state), 1;

  const auto& e = it->second;
  if (e.type == type::axis)
    return push_gamepad_axis(state, e.axis);
  return push_gamepad_button(state, e.button);
}

void gamepad::wire() {
  SDL_AddEventWatch(on_event, nullptr);

  auto count = 0;
  const auto ids = std::unique_ptr<SDL_JoystickID[], SDL_Deleter>{SDL_GetGamepads(&count)};
  if (ids && count > 0) {
    ptr.reset(SDL_OpenGamepad(ids[0]));
  }

  metatable(L, "Gamepad", gamepad_index);

  singleton(L, "Gamepad", "gamepad");
}
