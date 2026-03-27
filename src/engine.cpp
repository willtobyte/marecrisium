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
  const auto fullscreen = lua_isboolean(L, -1) ? lua_toboolean(L, -1) != 0 : false;
  lua_pop(L, 1);

  b2SetLengthUnitsPerMeter(100.f);

  {
    lua_getfield(L, -1, "ticks");
    const auto ticks = lua_isnumber(L, -1) ? static_cast<int>(lua_tonumber(L, -1)) : 0;
    lua_pop(L, 1);
    if (ticks > 0)
      _tick_interval = 1.f / static_cast<float>(ticks);
  }

  lua_getfield(L, -1, "title");
  const auto *title_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
  const auto title = title_raw ? std::string_view{title_raw} : std::string_view{"Untitled"};

#ifndef DEBUG
  lua_getfield(L, -2, "sentry");
  const std::string dsn = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
  lua_pop(L, 1);

  auto *const options = sentry_options_new();
  sentry_options_set_dsn(options, dsn.data());
  sentry_options_set_database_path(options, ".sentry");
  sentry_options_set_sample_rate(options, 1.);

  sentry_options_add_attachment(options, "cassette.tape");
  sentry_options_add_attachment(options, "stdout.txt");
  sentry_options_add_attachment(options, "stderr.txt");
  sentry_options_add_attachment(options, "VERSION");

  sentry_init(options);
  std::atexit(+[] { sentry_close(); });
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

  {
    lua_getfield(L, -1, "splash");

    if (lua_isstring(L, -1)) [[likely]] {
      const auto filename = std::format("blobs/splashes/{}.png", lua_tostring(L, -1));

      const pixmap splash{filename};

      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);

      const auto srcw = static_cast<float>(splash.width());
      const auto srch = static_cast<float>(splash.height());
      const auto dstw = static_cast<float>(width) / scale;
      const auto dsth = static_cast<float>(height) / scale;

      splash.draw(0, 0, srcw, srch, 0, 0, dstw, dsth);

      SDL_RenderPresent(renderer);
    }

    lua_pop(L, 1);
  }

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
  auto on_begin = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  if (on_begin != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, on_begin);
    pcall(L, 0, 0);
    luaL_unref(L, LUA_REGISTRYINDEX, on_begin);
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

  // websocket::poll();

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
