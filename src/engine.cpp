#include "engine.hpp"

lua_State *L = nullptr;
ma_engine *audioengine = nullptr;
SDL_Renderer *renderer = nullptr;
struct viewport viewport{};
struct resources* depot = nullptr;

static constexpr std::array<SDL_EventType, 46> disabled_events{
  SDL_EVENT_KEY_DOWN,
  SDL_EVENT_TEXT_EDITING,
  SDL_EVENT_TEXT_EDITING_CANDIDATES,
  SDL_EVENT_KEYMAP_CHANGED,
  SDL_EVENT_MOUSE_MOTION,
  SDL_EVENT_MOUSE_BUTTON_DOWN,
  SDL_EVENT_MOUSE_BUTTON_UP,
  SDL_EVENT_MOUSE_WHEEL,
  SDL_EVENT_GAMEPAD_AXIS_MOTION,
  SDL_EVENT_GAMEPAD_BUTTON_DOWN,
  SDL_EVENT_GAMEPAD_BUTTON_UP,
  SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN,
  SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION,
  SDL_EVENT_GAMEPAD_TOUCHPAD_UP,
  SDL_EVENT_GAMEPAD_SENSOR_UPDATE,
  SDL_EVENT_GAMEPAD_UPDATE_COMPLETE,
  SDL_EVENT_JOYSTICK_AXIS_MOTION,
  SDL_EVENT_JOYSTICK_BALL_MOTION,
  SDL_EVENT_JOYSTICK_HAT_MOTION,
  SDL_EVENT_JOYSTICK_BUTTON_DOWN,
  SDL_EVENT_JOYSTICK_BUTTON_UP,
  SDL_EVENT_JOYSTICK_UPDATE_COMPLETE,
  SDL_EVENT_JOYSTICK_ADDED,
  SDL_EVENT_JOYSTICK_REMOVED,
  SDL_EVENT_FINGER_DOWN,
  SDL_EVENT_FINGER_UP,
  SDL_EVENT_FINGER_MOTION,
  SDL_EVENT_FINGER_CANCELED,
  SDL_EVENT_CLIPBOARD_UPDATE,
  SDL_EVENT_DROP_FILE,
  SDL_EVENT_DROP_TEXT,
  SDL_EVENT_DROP_BEGIN,
  SDL_EVENT_DROP_COMPLETE,
  SDL_EVENT_DROP_POSITION,
  SDL_EVENT_PEN_PROXIMITY_IN,
  SDL_EVENT_PEN_PROXIMITY_OUT,
  SDL_EVENT_PEN_DOWN,
  SDL_EVENT_PEN_UP,
  SDL_EVENT_PEN_BUTTON_DOWN,
  SDL_EVENT_PEN_BUTTON_UP,
  SDL_EVENT_PEN_MOTION,
  SDL_EVENT_PEN_AXIS,
  SDL_EVENT_CAMERA_DEVICE_ADDED,
  SDL_EVENT_CAMERA_DEVICE_REMOVED,
  SDL_EVENT_CAMERA_DEVICE_APPROVED,
  SDL_EVENT_CAMERA_DEVICE_DENIED,
};

engine::engine() {
  const auto buffer = io::read("scripts/main.lua");
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();

  if (luaL_loadbuffer(L, data, size, "@main.lua") != 0 || lua_pcall(L, 0, 1, 0) != 0) {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error{std::move(error)};
  }

  lua_getfield(L, -1, "width");
  const auto width = lua_isnumber(L, -1) ? static_cast<int>(lua_tonumber(L, -1)) : 1920;
  lua_pop(L, 1);

  lua_getfield(L, -1, "height");
  const auto height = lua_isnumber(L, -1) ? static_cast<int>(lua_tonumber(L, -1)) : 1080;
  lua_pop(L, 1);

  lua_getfield(L, -1, "scale");
  const auto scale = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 1.f;
  lua_pop(L, 1);

  lua_getfield(L, -1, "fullscreen");
  const auto fullscreen = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : 0;
  lua_pop(L, 1);

  b2SetLengthUnitsPerMeter(100.f);

  lua_getfield(L, -1, "ticks");
  if (lua_isnumber(L, -1)) {
    const auto ticks = static_cast<int>(lua_tonumber(L, -1));
    if (ticks > 0)
      _tick_interval = 1.f / static_cast<float>(ticks);
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "title");
  const std::string_view title = lua_isstring(L, -1) ? lua_tostring(L, -1) : "Untitled";

#ifndef DEBUG
  lua_getfield(L, -2, "sentry");
  if (lua_isstring(L, -1)) {
    const std::string_view dsn = lua_tostring(L, -1);
    if (!dsn.empty()) {
      auto* const options = sentry_options_new();
      sentry_options_set_dsn(options, dsn.data());
      sentry_options_set_database_path(options, ".sentry");
      sentry_options_set_sample_rate(options, 1.);

      sentry_options_add_attachment(options, "cassette.tape");
      sentry_options_add_attachment(options, "stdout.txt");
      sentry_options_add_attachment(options, "stderr.txt");

      sentry_init(options);
      std::atexit([] { sentry_close(); });
    }
  }
  lua_pop(L, 1);
#endif

  static const auto window = SDL_CreateWindow(
    title.data(), width, height, fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
  lua_pop(L, 1);

  const auto vsync = std::getenv("NOVSYNC") ? 0 : 1;
  const auto properties = SDL_CreateProperties();
  SDL_SetPointerProperty(properties, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
  SDL_SetNumberProperty(properties, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, vsync);
  SDL_SetStringProperty(properties, SDL_PROP_RENDERER_CREATE_NAME_STRING, nullptr);

  renderer = SDL_CreateRendererWithProperties(properties);

  SDL_SetRenderLogicalPresentation(renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_SetRenderScale(renderer, scale, scale);

  SDL_RaiseWindow(window);

  viewport = {
    static_cast<float>(width) / scale,
    static_cast<float>(height) / scale,
    scale,
    .0f,
    .0f
  };

  lua_newtable(L);
  lua_pushnumber(L, static_cast<lua_Number>(viewport.width));
  lua_setfield(L, -2, "width");
  lua_pushnumber(L, static_cast<lua_Number>(viewport.height));
  lua_setfield(L, -2, "height");
  lua_pushnumber(L, static_cast<lua_Number>(viewport.scale));
  lua_setfield(L, -2, "scale");
  lua_setglobal(L, "viewport");

  _director.wire();

  lua_getfield(L, -1, "on_begin");
  if (lua_isfunction(L, -1)) {
    if (lua_pcall(L, 0, 0, 0) != 0) {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 1);
      throw std::runtime_error{std::move(error)};
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);

  for (const auto type : disabled_events)
    SDL_SetEventEnabled(type, false);
}

void engine::run() {
  while (_running) [[likely]] {
    loop();
  }
}

void engine::loop() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_EVENT_QUIT: {
        _running = false;
      } break;

      case SDL_EVENT_KEY_UP: {
        switch (event.key.key) {
          case SDLK_F11: {
            auto *const window = SDL_GetRenderWindow(renderer);
            const auto fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
            SDL_SetWindowFullscreen(window, !fullscreen);
          } break;

          default:
            break;
        }
      } break;

      default:
        break;
    }
  }

  const auto now = SDL_GetPerformanceCounter();
  static auto prior = now;
  static const auto frequency = static_cast<double>(SDL_GetPerformanceFrequency());
  const auto delta = std::min(static_cast<float>(static_cast<double>(now - prior) / frequency), 1.f / 30.f);
  prior = now;

  static auto tick = now;
  static auto frames = 0;
  ++frames;
  const auto elapsed = static_cast<double>(now - tick) / frequency;

  if (elapsed >= 1.0) {
    const auto fps = frames / elapsed;
    const auto memory = lua_gc(L, LUA_GCCOUNT, 0);
    std::println("{:.1f} {}KB", fps, memory);
    frames = 0;
    tick = now;
  }

  lua_gc(L, LUA_GCSTEP, 100);

  websocket::poll();

  _director.transition();

  if (_tick_interval > .0f) {
    _tick_accumulator += delta;

    while (_tick_accumulator >= _tick_interval) {
      _tick_accumulator -= _tick_interval;
      _director.on_tick(++_tick);
    }
  }

  _director.update(delta);

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderClear(renderer);

  _director.draw();

  SDL_RenderPresent(renderer);

  SteamAPI_RunCallbacks();
}
