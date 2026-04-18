#include <SDL3/SDL_main.h>

int main(int argc, char **argv) {
#ifndef DEBUG
  if (auto* out = std::freopen("stdout.txt", "w", stdout)) {
    setvbuf(out, nullptr, _IONBF, 0);
  }

  if (auto* err = std::freopen("stderr.txt", "w", stderr)) {
    setvbuf(err, nullptr, _IONBF, 0);
  }
#endif

  // SDL_SetHint(SDL_HINT_MAC_PRESS_AND_HOLD, "0");
  SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_VIDEO);

  PHYSFS_init(argv[0]);
  PHYSFS_registerArchiver(&archiver);

  ma_engine engine;
  auto config = ma_engine_config_init();
  config.channels = 2;
  config.sampleRate = 48000;
  config.periodSizeInFrames = 2048;
  ma_engine_init(&config, &engine);
  audioengine = &engine;

  L = luaL_newstate();
  luaL_openlibs(L);

  SteamAPI_InitSafe();

  int result;
  {
    resources resources;
    depot = &resources;

    application app;
    result = app.run();
  }

  SteamAPI_Shutdown();

  lua_close(L);

  ma_engine_stop(&engine);
  ma_engine_uninit(&engine);

  PHYSFS_deinit();

  SDL_Quit();

  return result;
}
