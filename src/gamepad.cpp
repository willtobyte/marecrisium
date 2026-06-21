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

static float deadzone(Sint16 axis) {
  const auto normalized = static_cast<float>(axis) / 32768.f;
  const auto magnitude = std::abs(normalized);
  if (magnitude < DEADZONE_THRESHOLD) [[likely]]
    return .0f;

  const auto sign = std::copysign(1.f, normalized);
  return sign * (magnitude - DEADZONE_THRESHOLD) / (1.f - DEADZONE_THRESHOLD);
}

static std::atomic<SDL_Gamepad *> ptr{nullptr};

static bool on_event(void *, SDL_Event *event) {
  switch (event->type) {
    case SDL_EVENT_GAMEPAD_ADDED: {
      if (!ptr.load()) [[unlikely]] {
        auto *opened = SDL_OpenGamepad(event->gdevice.which);
        SDL_Gamepad *expected = nullptr;
        if (!ptr.compare_exchange_strong(expected, opened))
          SDL_CloseGamepad(opened);
      }
    } break;

    case SDL_EVENT_GAMEPAD_REMOVED: {
      auto *const gamepad = ptr.load();
      if (gamepad && SDL_GetGamepadID(gamepad) == event->gdevice.which) [[likely]]
        SDL_CloseGamepad(ptr.exchange(nullptr));
    } break;

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

  auto *const gamepad = ptr.load();
  if (!gamepad) [[unlikely]] {
    lua_pushboolean(state, 0);
    return 1;
  }

  lua_pushboolean(state, static_cast<bool>(SDL_RumbleGamepad(gamepad, low16, high16, duration)) ? 1 : 0);
  return 1;
}

static int gamepad_led(lua_State *state) {
  const auto r = static_cast<uint8_t>(std::clamp(static_cast<float>(luaL_checknumber(state, 2)), .0f, 1.f) * 255.f);
  const auto g = static_cast<uint8_t>(std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 1.f) * 255.f);
  const auto b = static_cast<uint8_t>(std::clamp(static_cast<float>(luaL_checknumber(state, 4)), .0f, 1.f) * 255.f);

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

static int _rumble_reference = LUA_NOREF;
static int _led_reference = LUA_NOREF;

static int gamepad_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};
  auto *const gamepad = ptr.load();

  if (const auto it = axes.find(id); it != axes.end()) [[likely]]
    return push_gamepad_axis(state, it->second, gamepad);

  if (const auto it = buttons.find(id); it != buttons.end()) [[likely]]
    return push_gamepad_button(state, it->second, gamepad);

  switch (id) {
    case property::connected:
      lua_pushboolean(state, gamepad ? 1 : 0);
      return 1;

    case property::rumble:
      lua_rawgeti(state, LUA_REGISTRYINDEX, _rumble_reference);
      return 1;

    case property::led:
      lua_rawgeti(state, LUA_REGISTRYINDEX, _led_reference);
      return 1;

    case property::name:
      lua_pushstring(state, gamepad ? SDL_GetGamepadName(gamepad) : "");
      return 1;

    default:
      lua_pushnil(state);
      return 1;
  }
}

void gamepad::wire() {
  SDL_AddEventWatch(on_event, nullptr);

  auto count = 0;
  const auto gamepads = std::unique_ptr<SDL_JoystickID[], SDL_Deleter>{SDL_GetGamepads(&count)};
  if (gamepads && count > 0) [[likely]] {
    ptr.store(SDL_OpenGamepad(gamepads[0]));
  }

  cfunction(L, gamepad_rumble);
  _rumble_reference = luaL_ref(L, LUA_REGISTRYINDEX);
  cfunction(L, gamepad_led);
  _led_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  metatable(L, "Gamepad", gamepad_index);

  singleton(L, "Gamepad", "gamepad");
}
