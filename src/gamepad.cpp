namespace {
  namespace lookup {
    constexpr auto connected = "connected"_hs;
    constexpr auto rumble = "rumble"_hs;
    constexpr auto led = "led"_hs;
    constexpr auto name = "name"_hs;
    constexpr auto left_x = "left_x"_hs;
    constexpr auto left_y = "left_y"_hs;
    constexpr auto right_x = "right_x"_hs;
    constexpr auto right_y = "right_y"_hs;
    constexpr auto trigger_left = "trigger_left"_hs;
    constexpr auto trigger_right = "trigger_right"_hs;
    constexpr auto south = "south"_hs;
    constexpr auto east = "east"_hs;
    constexpr auto west = "west"_hs;
    constexpr auto north = "north"_hs;
    constexpr auto back = "back"_hs;
    constexpr auto guide = "guide"_hs;
    constexpr auto start = "start"_hs;
    constexpr auto shoulder_left = "shoulder_left"_hs;
    constexpr auto shoulder_right = "shoulder_right"_hs;
    constexpr auto stick_left = "stick_left"_hs;
    constexpr auto stick_right = "stick_right"_hs;
    constexpr auto up = "up"_hs;
    constexpr auto down = "down"_hs;
    constexpr auto left = "left"_hs;
    constexpr auto right = "right"_hs;
  }

  struct reference final {
    static int rumble;
    static int led;
  };

  int reference::rumble = LUA_NOREF;
  int reference::led = LUA_NOREF;
}

static SDL_GamepadAxis axis(entt::id_type id) {
  switch (id) {
    case lookup::left_x: return SDL_GAMEPAD_AXIS_LEFTX;
    case lookup::left_y: return SDL_GAMEPAD_AXIS_LEFTY;
    case lookup::right_x: return SDL_GAMEPAD_AXIS_RIGHTX;
    case lookup::right_y: return SDL_GAMEPAD_AXIS_RIGHTY;
    case lookup::trigger_left: return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
    case lookup::trigger_right: return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
    default: return SDL_GAMEPAD_AXIS_INVALID;
  }
}

static SDL_GamepadButton button(entt::id_type id) {
  switch (id) {
    case lookup::south: return SDL_GAMEPAD_BUTTON_SOUTH;
    case lookup::east: return SDL_GAMEPAD_BUTTON_EAST;
    case lookup::west: return SDL_GAMEPAD_BUTTON_WEST;
    case lookup::north: return SDL_GAMEPAD_BUTTON_NORTH;
    case lookup::back: return SDL_GAMEPAD_BUTTON_BACK;
    case lookup::guide: return SDL_GAMEPAD_BUTTON_GUIDE;
    case lookup::start: return SDL_GAMEPAD_BUTTON_START;
    case lookup::shoulder_left: return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    case lookup::shoulder_right: return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    case lookup::stick_left: return SDL_GAMEPAD_BUTTON_LEFT_STICK;
    case lookup::stick_right: return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    case lookup::up: return SDL_GAMEPAD_BUTTON_DPAD_UP;
    case lookup::down: return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    case lookup::left: return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    case lookup::right: return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    default: return SDL_GAMEPAD_BUTTON_INVALID;
  }
}

static constexpr auto threshold = .1f;

static float deadzone(Sint16 value) {
  constexpr auto range = -static_cast<float>(std::numeric_limits<Sint16>::min());
  const auto normalized = static_cast<float>(value) / range;
  const auto magnitude = std::abs(normalized);
  if (magnitude < threshold) [[likely]]
    return .0f;

  const auto sign = std::copysign(1.f, normalized);
  return sign * (magnitude - threshold) / (1.f - threshold);
}

static std::atomic<SDL_Gamepad *> ptr{nullptr};

static void connect(SDL_JoystickID id) {
  if (ptr.load()) [[likely]] return;

  auto *const candidate = SDL_OpenGamepad(id);
  if (!candidate) [[unlikely]] return;

  SDL_Gamepad *expected = nullptr;
  if (!ptr.compare_exchange_strong(expected, candidate))
    SDL_CloseGamepad(candidate);
}

static void connect() {
  auto count = 0;
  const auto gamepads = std::unique_ptr<SDL_JoystickID[], SDL_Deleter>{SDL_GetGamepads(&count)};
  if (gamepads && count > 0) [[likely]]
    connect(gamepads[0]);
}

static bool on_event(void *, SDL_Event *event) {
  switch (event->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
      connect(event->gdevice.which);
      break;

    case SDL_EVENT_GAMEPAD_REMOVED: {
      auto *const gamepad = ptr.load();
      if (gamepad && SDL_GetGamepadID(gamepad) == event->gdevice.which) [[likely]] {
        SDL_CloseGamepad(ptr.exchange(nullptr));
        connect();
      }
    } break;

    default:
      break;
  }

  return true;
}

static int gamepad_rumble(lua_State *state) {
  const auto low = std::clamp(static_cast<float>(luaL_checknumber(state, 2)), .0f, 1.f);
  const auto high = std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 1.f);
  const auto milliseconds = luaL_checkinteger(state, 4);
  const auto finite = std::isfinite(low) && std::isfinite(high);
  const auto valid = milliseconds >= 0 && static_cast<uint64_t>(milliseconds) <= std::numeric_limits<uint32_t>::max();
  [[assume(finite)]];
  [[assume(valid)]];

  const auto duration = static_cast<uint32_t>(milliseconds);
  const auto lo = static_cast<uint16_t>(low * static_cast<float>(std::numeric_limits<uint16_t>::max()));
  const auto hi = static_cast<uint16_t>(high * static_cast<float>(std::numeric_limits<uint16_t>::max()));

  auto *const gamepad = ptr.load();
  if (!gamepad) [[unlikely]] {
    lua_pushboolean(state, 0);
    return 1;
  }

  lua_pushboolean(state, static_cast<bool>(SDL_RumbleGamepad(gamepad, lo, hi, duration)) ? 1 : 0);
  return 1;
}

static int gamepad_led(lua_State *state) {
  const auto red = std::clamp(static_cast<float>(luaL_checknumber(state, 2)), .0f, 1.f);
  const auto green = std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 1.f);
  const auto blue = std::clamp(static_cast<float>(luaL_checknumber(state, 4)), .0f, 1.f);
  const auto finite = std::isfinite(red) && std::isfinite(green) && std::isfinite(blue);
  [[assume(finite)]];

  constexpr auto range = static_cast<float>(std::numeric_limits<uint8_t>::max());
  const auto r = static_cast<uint8_t>(red * range);
  const auto g = static_cast<uint8_t>(green * range);
  const auto b = static_cast<uint8_t>(blue * range);

  auto *const gamepad = ptr.load();
  if (!gamepad) [[unlikely]] {
    lua_pushboolean(state, 0);
    return 1;
  }

  lua_pushboolean(state, static_cast<bool>(SDL_SetGamepadLED(gamepad, r, g, b)) ? 1 : 0);
  return 1;
}

static int push_gamepad_axis(lua_State *state, SDL_GamepadAxis axis, SDL_Gamepad *gamepad) {
  if (!gamepad) [[unlikely]]
    return lua_pushnumber(state, .0), 1;

  lua_pushnumber(state, static_cast<lua_Number>(deadzone(SDL_GetGamepadAxis(gamepad, axis))));
  return 1;
}

static int push_gamepad_button(lua_State *state, SDL_GamepadButton button, SDL_Gamepad *gamepad) {
  if (!gamepad) [[unlikely]]
    return lua_pushboolean(state, 0), 1;

  lua_pushboolean(state, static_cast<bool>(SDL_GetGamepadButton(gamepad, button)) ? 1 : 0);
  return 1;
}

static int gamepad_index(lua_State *state) {
  const auto id = entt::hashed_string::value(luaL_checkstring(state, 2));
  auto *const gamepad = ptr.load();

  if (const auto value = axis(id); value != SDL_GAMEPAD_AXIS_INVALID) [[likely]]
    return push_gamepad_axis(state, value, gamepad);

  if (const auto value = button(id); value != SDL_GAMEPAD_BUTTON_INVALID) [[likely]]
    return push_gamepad_button(state, value, gamepad);

  switch (id) {
    case lookup::connected:
      lua_pushboolean(state, gamepad ? 1 : 0);
      return 1;

    case lookup::rumble:
      lua_rawgeti(state, LUA_REGISTRYINDEX, reference::rumble);
      return 1;

    case lookup::led:
      lua_rawgeti(state, LUA_REGISTRYINDEX, reference::led);
      return 1;

    case lookup::name: {
      const auto *value = gamepad ? SDL_GetGamepadName(gamepad) : nullptr;
      lua_pushstring(state, value ? value : "");
      return 1;
    }

    default:
      lua_pushnil(state);
      return 1;
  }
}

void gamepad::wire() {
  SDL_AddEventWatch(on_event, nullptr);
  connect();

  binding::callback(L, gamepad_rumble);
  reference::rumble = luaL_ref(L, LUA_REGISTRYINDEX);
  binding::callback(L, gamepad_led);
  reference::led = luaL_ref(L, LUA_REGISTRYINDEX);

  binding::metatable(L, "Gamepad", gamepad_index);

  binding::singleton(L, "Gamepad", "gamepad");
}
