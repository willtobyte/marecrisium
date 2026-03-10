#include "engine.hpp"

lua_State *L = nullptr;
ma_engine *audioengine = nullptr;
SDL_Renderer *renderer = nullptr;
struct viewport viewport{};
struct resources resources{};

engine::engine() {
  const auto buffer = io::read("scripts/main.lua");
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();

  if (luaL_loadbuffer(L, data, size, "@main.lua") != 0 || lua_pcall(L, 0, 1, 0) != 0) {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(std::move(error));
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

  static const auto window = SDL_CreateWindow(
    title.data(), width, height, fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
  lua_pop(L, 1);

  const auto vsync = std::getenv("NOVSYNC") ? 0 : 1;
  const auto properties = SDL_CreateProperties();
  SDL_SetPointerProperty(properties, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
  SDL_SetNumberProperty(properties, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, vsync);
  SDL_SetStringProperty(properties, SDL_PROP_RENDERER_CREATE_NAME_STRING, nullptr);

  renderer = SDL_CreateRendererWithProperties(properties);
  SDL_DestroyProperties(properties);

  SDL_SetRenderLogicalPresentation(renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_SetRenderScale(renderer, scale, scale);

  SDL_RaiseWindow(window);

  viewport = {
    static_cast<float>(width) / scale,
    static_cast<float>(height) / scale,
    scale
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
      throw std::runtime_error(std::move(error));
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
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

#ifdef DEVELOPMENT
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
#endif

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
