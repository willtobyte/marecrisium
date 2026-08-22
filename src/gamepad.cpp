static SDL_GamepadAxis axis(std::string_view key) {
  if (key == "left_x") return SDL_GAMEPAD_AXIS_LEFTX;
  if (key == "left_y") return SDL_GAMEPAD_AXIS_LEFTY;
  if (key == "right_x") return SDL_GAMEPAD_AXIS_RIGHTX;
  if (key == "right_y") return SDL_GAMEPAD_AXIS_RIGHTY;
  if (key == "trigger_left") return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
  if (key == "trigger_right") return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;

  return SDL_GAMEPAD_AXIS_INVALID;
}

static SDL_GamepadButton button(std::string_view key) {
  if (key == "south") return SDL_GAMEPAD_BUTTON_SOUTH;
  if (key == "east") return SDL_GAMEPAD_BUTTON_EAST;
  if (key == "west") return SDL_GAMEPAD_BUTTON_WEST;
  if (key == "north") return SDL_GAMEPAD_BUTTON_NORTH;
  if (key == "back") return SDL_GAMEPAD_BUTTON_BACK;
  if (key == "guide") return SDL_GAMEPAD_BUTTON_GUIDE;
  if (key == "start") return SDL_GAMEPAD_BUTTON_START;
  if (key == "shoulder_left") return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
  if (key == "shoulder_right") return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
  if (key == "stick_left") return SDL_GAMEPAD_BUTTON_LEFT_STICK;
  if (key == "stick_right") return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
  if (key == "up") return SDL_GAMEPAD_BUTTON_DPAD_UP;
  if (key == "down") return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
  if (key == "left") return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
  if (key == "right") return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;

  return SDL_GAMEPAD_BUTTON_INVALID;
}

static constexpr auto threshold = .1f;

static float deadzone(Sint16 value) {
  constexpr auto range = -static_cast<float>(std::numeric_limits<Sint16>::min());
  const auto normalized = static_cast<float>(value) / (value < 0 ? range : range - 1.f);
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
  if (!candidate) [[unlikely]]
    return;

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

static int rumble_callback(lua_State *state) {
  const auto low = std::clamp(std::fmax(static_cast<float>(luaL_checknumber(state, 2)), .0f), .0f, 1.f);
  const auto high = std::clamp(std::fmax(static_cast<float>(luaL_checknumber(state, 3)), .0f), .0f, 1.f);
  const auto duration = std::clamp(
    luaL_checkinteger(state, 4),
    lua_Integer{},
    static_cast<lua_Integer>(std::numeric_limits<uint32_t>::max()));
  const auto ms = static_cast<uint32_t>(duration);
  const auto lo = static_cast<uint16_t>(low * static_cast<float>(std::numeric_limits<uint16_t>::max()));
  const auto hi = static_cast<uint16_t>(high * static_cast<float>(std::numeric_limits<uint16_t>::max()));

  auto *const gamepad = ptr.load();
  if (!gamepad) [[unlikely]] {
    lua_pushboolean(state, 0);
    return 1;
  }

  lua_pushboolean(state, SDL_RumbleGamepad(gamepad, lo, hi, ms));
  return 1;
}

static int led_callback(lua_State *state) {
  const auto red = std::clamp(std::fmax(static_cast<float>(luaL_checknumber(state, 2)), .0f), .0f, 1.f);
  const auto green = std::clamp(std::fmax(static_cast<float>(luaL_checknumber(state, 3)), .0f), .0f, 1.f);
  const auto blue = std::clamp(std::fmax(static_cast<float>(luaL_checknumber(state, 4)), .0f), .0f, 1.f);

  constexpr auto range = static_cast<float>(std::numeric_limits<uint8_t>::max());
  const auto r = static_cast<uint8_t>(red * range);
  const auto g = static_cast<uint8_t>(green * range);
  const auto b = static_cast<uint8_t>(blue * range);

  auto *const gamepad = ptr.load();
  if (!gamepad) [[unlikely]] {
    lua_pushboolean(state, 0);
    return 1;
  }

  lua_pushboolean(state, SDL_SetGamepadLED(gamepad, r, g, b));
  return 1;
}

static int index(lua_State *state) {
  std::size_t length;
  const auto* data = luaL_checklstring(state, 2, &length);
  const std::string_view key{data, length};
  auto *const gamepad = ptr.load();

  if (const auto value = axis(key); value != SDL_GAMEPAD_AXIS_INVALID) [[likely]] {
    lua_pushnumber(state, gamepad
      ? static_cast<lua_Number>(deadzone(SDL_GetGamepadAxis(gamepad, value)))
      : lua_Number{});
    return 1;
  }

  if (const auto value = button(key); value != SDL_GAMEPAD_BUTTON_INVALID) [[likely]] {
    lua_pushboolean(state, gamepad && SDL_GetGamepadButton(gamepad, value));
    return 1;
  }

  if (key == "connected") {
    lua_pushboolean(state, gamepad != nullptr);
    return 1;
  }
  if (key == "name") {
    const auto *value = gamepad ? SDL_GetGamepadName(gamepad) : nullptr;
    lua_pushstring(state, value ? value : "");
    return 1;
  }

  lua_pushnil(state);
  return 1;
}

void gamepad::wire() {
  SDL_AddEventWatch(on_event, nullptr);
  connect();

  lua_newtable(L);
  lua_pushcfunction(L, rumble_callback);
  lua_setfield(L, -2, "rumble");
  lua_pushcfunction(L, led_callback);
  lua_setfield(L, -2, "led");

  lua_newtable(L);
  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, +[](lua_State*) -> int { return 0; });
  lua_setfield(L, -2, "__newindex");
  lua_setmetatable(L, -2);
  lua_setglobal(L, "gamepad");
}
