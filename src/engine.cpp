engine::engine() {
  const auto buffer = io::read("scripts/main.lua");
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(buffer.data()), buffer.size(), "@main.lua") != LUA_OK) [[unlikely]]
    lua_error(L);

  if (lua_pcall(L, 0, 1, 0) != LUA_OK) [[unlikely]]
    lua_error(L);

  lua_getfield(L, -1, "width");
  const auto width = static_cast<int>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "height");
  const auto height = static_cast<int>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "scale");
  const auto scale = static_cast<float>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "fullscreen");
  const auto fullscreen = lua_toboolean(L, -1) != 0;
  lua_pop(L, 1);

  lua_getfield(L, -1, "title");
  const auto* title = lua_tostring(L, -1);

  static const auto window = SDL_CreateWindow(
    title,
    width,
    height,
    fullscreen ? SDL_WINDOW_FULLSCREEN : 0
  );

  lua_pop(L, 1);

  const auto vsync = std::getenv("NOVSYNC") == nullptr;
  const auto properties = SDL_CreateProperties();
  SDL_SetPointerProperty(properties, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
  SDL_SetNumberProperty(properties, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, vsync);
  SDL_SetStringProperty(properties, SDL_PROP_RENDERER_CREATE_NAME_STRING, nullptr);

  renderer = SDL_CreateRendererWithProperties(properties);
  SDL_DestroyProperties(properties);

  SDL_SetRenderLogicalPresentation(renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_SetRenderScale(renderer, scale, scale);

  // lua_getfield(L, -1, "splash");
  // const auto filename = std::format("blobs/splashes/{}.png", lua_tostring(L, -1));
  // lua_pop(L, 1);

  // const pixmap splash{filename};

  // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  // SDL_RenderClear(renderer);

  // splash.draw(
  //   0, 0,
  //   static_cast<float>(splash.width()), static_cast<float>(splash.height()),
  //   0, 0,
  //   static_cast<float>(width) / scale, static_cast<float>(height) / scale
  // );

  SDL_RenderPresent(renderer);
  SDL_PumpEvents();

  SDL_RaiseWindow(window);
  SDL_FlashWindow(window, SDL_FLASH_UNTIL_FOCUSED);

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
  if (lua_pcall(L, 0, 0, 0) != LUA_OK) [[unlikely]]
    lua_error(L);

  lua_pop(L, 1);
}

void engine::run() {
  lua_gc(L, LUA_GCCOLLECT, 0);
  lua_gc(L, LUA_GCRESTART, 0);

  const auto elapsed = static_cast<double>(SDL_GetPerformanceCounter() - _boot) * 1'000.0 / static_cast<double>(SDL_GetPerformanceFrequency());

  std::println("[boot] ready in {:.2f}", elapsed);

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

  _director.transition();

  _director.update(delta);

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderClear(renderer);

  _director.draw();

  SDL_RenderPresent(renderer);

  SteamAPI_RunCallbacks();
}
