#include <SDL3/SDL_main.h>

int main(int argc, char** argv) {
#ifndef DEBUG
  if (auto* out = std::freopen("stdout.txt", "w", stdout)) {
    std::setvbuf(out, nullptr, _IONBF, 0);
  }

  if (auto* err = std::freopen("stderr.txt", "w", stderr)) {
    std::setvbuf(err, nullptr, _IONBF, 0);
  }
#endif

  SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_VIDEO);
  std::atexit(+[] { SDL_Quit(); });

  PHYSFS_init(argv[0]);
  PHYSFS_registerArchiver(&archiver);
  std::atexit(+[] { PHYSFS_deinit(); });

  auto config = ma_engine_config_init();
  config.channels = 2;
  config.sampleRate = 48000;
  config.periodSizeInFrames = 2048;
  ma_engine_init(&config, &audio);
  std::atexit(+[] { ma_engine_uninit(&audio); });

  L = luaL_newstate();
  luaL_openlibs(L);
  std::atexit(+[] { lua_close(L); });

  SteamAPI_InitSafe();
  std::atexit(+[] { SteamAPI_Shutdown(); });

  resources storage;
  depot = &storage;

  application app;
  return app.run();
}
