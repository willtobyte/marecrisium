#include <SDL3/SDL_main.h>

static_assert(std::endian::native == std::endian::little, "this engine targets little-endian platforms only");

int main(int, char**) {
#ifndef DEBUG
  if (auto* out = std::freopen("stdout.txt", "w", stdout)) {
    std::setvbuf(out, nullptr, _IONBF, 0);
  }

  if (auto* err = std::freopen("stderr.txt", "w", stderr)) {
    std::setvbuf(err, nullptr, _IONBF, 0);
  }
#endif

  SDL_SetHint(SDL_HINT_MAC_PRESS_AND_HOLD, "0");
  SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_VIDEO);
  std::atexit([]{ SDL_Quit(); });

  auto config = ma_engine_config_init();
  config.channels = 2;
  config.periodSizeInFrames = 2'048;
  ma_engine_init(&config, &audio);
  std::atexit([]{ ma_engine_uninit(&audio); });

  L = luaL_newstate();
  luaL_openlibs(L);
  lua_atpanic(L, +[](lua_State* state) -> int {
    const auto* message = lua_tostring(state, -1);
    auto exception = std::runtime_error{message ? message : "unknown lua error"};
    lua_pop(state, 1);
    throw exception;
  });
  lua_pushcfunction(L, traceback::build);
  lua_rawseti(L, LUA_REGISTRYINDEX, traceback::slot);
  std::atexit([]{ lua_close(L); });

  SteamAPI_InitSafe();
  std::atexit([]{ SteamAPI_Shutdown(); });

  resources storage;
  depot = &storage;

  application app;
  return app.run();
}
