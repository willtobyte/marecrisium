#include "engine.hpp"

static constexpr std::array<SDL_EventType, 5> disabled_events{
  SDL_EVENT_KEY_DOWN,
  SDL_EVENT_MOUSE_MOTION,
  SDL_EVENT_MOUSE_BUTTON_DOWN,
  SDL_EVENT_MOUSE_BUTTON_UP,
  SDL_EVENT_MOUSE_WHEEL,
};

engine::engine() {
  const auto buffer = io::read("scripts/main.lua");
  compile(L, buffer, "@main.lua");

  pcall(L, 0, 1);

  const auto width = property<int>(L, -1, "width", 1920);
  const auto height = property<int>(L, -1, "height", 1080);
  const auto scale = property<float>(L, -1, "scale", 1.f);
  const auto fullscreen = property<bool>(L, -1, "fullscreen");

  b2SetLengthUnitsPerMeter(100.f);

  {
    const auto ticks = property<int>(L, -1, "ticks");
    if (ticks > 0)
      _tick_interval = 1.f / static_cast<float>(ticks);
  }

  const auto title = property<std::string_view>(L, -1, "title", "Untitled");

#ifndef DEBUG
  {
    const auto dsn = property<std::string_view>(L, -1, "sentry");
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
#endif

  static const auto window = SDL_CreateWindow(
    title.data(), width, height, fullscreen ? SDL_WINDOW_FULLSCREEN : 0);

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
  push(L, viewport.width);
  lua_setfield(L, -2, "width");
  push(L, viewport.height);
  lua_setfield(L, -2, "height");
  push(L, viewport.scale);
  lua_setfield(L, -2, "scale");
  lua_setglobal(L, "viewport");

  _director.wire();

  auto on_begin = acquire(L, -1, "on_begin");
  invoke(L, on_begin, LUA_NOREF);
  release(L, on_begin);

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
      case SDL_EVENT_QUIT:
        _running = false;
        break;

      case SDL_EVENT_KEY_UP:
        switch (event.key.key) {
          case SDLK_F11: {
            auto *const window = SDL_GetRenderWindow(renderer);
            const auto fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
            SDL_SetWindowFullscreen(window, !fullscreen);
          } break;

          default:
            break;
        }
        break;

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
