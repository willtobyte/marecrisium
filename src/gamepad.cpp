static keyring<
  "left_x",
  "left_y",
  "right_x",
  "right_y",
  "trigger_left",
  "trigger_right",
  "south",
  "east",
  "west",
  "north",
  "back",
  "guide",
  "start",
  "shoulder_left",
  "shoulder_right",
  "stick_left",
  "stick_right",
  "up",
  "down",
  "left",
  "right",
  "connected",
  "name"
> keys;

static constexpr auto threshold = .1f;

static float deadzone(Sint16 value) {
  constexpr auto range = -std::numeric_limits<Sint16>::min();
  const auto normalized = value / (value < 0 ? range : range - 1.f);
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
  const auto lo = static_cast<uint16_t>(low * std::numeric_limits<uint16_t>::max());
  const auto hi = static_cast<uint16_t>(high * std::numeric_limits<uint16_t>::max());

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

  constexpr auto range = std::numeric_limits<uint8_t>::max();
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

static int axis(lua_State *state, SDL_Gamepad *gamepad, SDL_GamepadAxis value) {
  lua_pushnumber(state, gamepad
    ? static_cast<lua_Number>(deadzone(SDL_GetGamepadAxis(gamepad, value)))
    : lua_Number{});

  return 1;
}

static int button(lua_State *state, SDL_Gamepad *gamepad, SDL_GamepadButton value) {
  lua_pushboolean(state, gamepad && SDL_GetGamepadButton(gamepad, value));

  return 1;
}

static int index(lua_State *state) {
  auto *const gamepad = ptr.load();

  switch (keys.find(lua_tostring(state, 2))) {
  case keys.id<"left_x">(): return axis(state, gamepad, SDL_GAMEPAD_AXIS_LEFTX);
  case keys.id<"left_y">(): return axis(state, gamepad, SDL_GAMEPAD_AXIS_LEFTY);
  case keys.id<"right_x">(): return axis(state, gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
  case keys.id<"right_y">(): return axis(state, gamepad, SDL_GAMEPAD_AXIS_RIGHTY);
  case keys.id<"trigger_left">(): return axis(state, gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
  case keys.id<"trigger_right">(): return axis(state, gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
  case keys.id<"south">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
  case keys.id<"east">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_EAST);
  case keys.id<"west">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_WEST);
  case keys.id<"north">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_NORTH);
  case keys.id<"back">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_BACK);
  case keys.id<"guide">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_GUIDE);
  case keys.id<"start">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_START);
  case keys.id<"shoulder_left">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
  case keys.id<"shoulder_right">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
  case keys.id<"stick_left">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
  case keys.id<"stick_right">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
  case keys.id<"up">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
  case keys.id<"down">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
  case keys.id<"left">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
  case keys.id<"right">(): return button(state, gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);

  case keys.id<"connected">():
    lua_pushboolean(state, gamepad != nullptr);

    return 1;

  case keys.id<"name">(): {
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
  keys.intern();

  SDL_AddEventWatch(on_event, nullptr);
  connect();

  lua_createtable(L, 0, 2);
  lua_pushcfunction(L, rumble_callback);
  lua_setfield(L, -2, "rumble");
  lua_pushcfunction(L, led_callback);
  lua_setfield(L, -2, "led");

  lua_createtable(L, 0, 2);
  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, +[](lua_State*) -> int { return 0; });
  lua_setfield(L, -2, "__newindex");
  lua_setmetatable(L, -2);
  lua_setglobal(L, "gamepad");
}
