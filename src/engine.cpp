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

  lua_getfield(L, -1, "ticks");
  const auto ticks = lua_isnumber(L, -1) ? static_cast<int>(lua_tonumber(L, -1)) : 0;
  lua_pop(L, 1);
  if (ticks > 0) [[likely]]
    _tick_interval = 1.f / static_cast<float>(ticks);

  lua_getfield(L, -1, "title");
  const std::string title = lua_isstring(L, -1) ? lua_tostring(L, -1) : "Untitled";
  lua_pop(L, 1);

  static const auto window = SDL_CreateWindow(
    title.data(),
    width,
    height,
    fullscreen ? SDL_WINDOW_FULLSCREEN : 0
  );

  const auto vsync = std::getenv("NOVSYNC") == nullptr;
  const auto properties = SDL_CreateProperties();
  SDL_SetPointerProperty(properties, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
  SDL_SetNumberProperty(properties, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, vsync);
  SDL_SetStringProperty(properties, SDL_PROP_RENDERER_CREATE_NAME_STRING, nullptr);

  renderer = SDL_CreateRendererWithProperties(properties);
  SDL_DestroyProperties(properties);

  SDL_SetRenderLogicalPresentation(renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_SetRenderScale(renderer, scale, scale);

  SDL_RaiseWindow(window);

  lua_getfield(L, -1, "splash");
  if (lua_isstring(L, -1)) [[likely]] {
    const auto filename = std::format("blobs/splashes/{}.png", lua_tostring(L, -1));
    const pixmap splash{filename};

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    splash.draw(
      0, 0, splash.width(), splash.height(),
      0, 0, width / scale, height / scale
    );

    SDL_RenderPresent(renderer);
    SDL_PumpEvents();
  }
  lua_pop(L, 1);

  #ifndef DEBUG
    lua_getfield(L, -1, "sentry");
    const std::string dsn = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);

    std::string version;
    std::getline(std::ifstream{"VERSION"}, version);

    auto *const options = sentry_options_new();
    sentry_options_set_dsn(options, dsn.data());
    sentry_options_set_database_path(options, ".sentry");
    sentry_options_set_sample_rate(options, 1.);
    sentry_options_set_release(options, version.c_str());

    sentry_options_add_attachment(options, "cassette.tape");
    sentry_options_add_attachment(options, "stdout.txt");
    sentry_options_add_attachment(options, "stderr.txt");

    sentry_init(options);
    std::atexit(+[] { sentry_close(); });
  #endif

  viewport = {
    width / scale,
    height / scale,
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

  b2SetLengthUnitsPerMeter(100.f);

  _director.wire();

  lua_getfield(L, -1, "on_begin");
  if (lua_isnoneornil(L, -1)) {
    lua_pop(L, 1);
  } else {
    pcall(L, 0, 0);
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

  if (elapsed >= 1.0) [[unlikely]] {
    const auto fps = frames / elapsed;
    const auto memory = lua_gc(L, LUA_GCCOUNT, 0);
    std::println("{:.1f} {}KB", fps, memory);
    frames = 0;
    tick = now;
  }

  lua_gc(L, LUA_GCSTEP, lua_gc(L, LUA_GCCOUNT, 0) > 4096 ? 240 : 80);

  _director.transition();

  if (_tick_interval > .0f) [[likely]] {
    _tick_accumulator += delta;

    while (_tick_accumulator >= _tick_interval) {
      _tick_accumulator -= _tick_interval;
      _director.on_tick(++_tick);
    }
  }

  _director.update(delta);

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  _director.draw();

  SDL_RenderPresent(renderer);

  SteamAPI_RunCallbacks();
}
