#include <SDL3/SDL_main.h>

int main(int argc, char **argv) {
#ifndef DEVELOPMENT
  if (auto* out = std::freopen("stdout.txt", "w", stdout)) {
    setvbuf(out, nullptr, _IONBF, 0);
  }

  if (auto* err = std::freopen("stderr.txt", "w", stderr)) {
    setvbuf(err, nullptr, _IONBF, 0);
  }
#endif

  SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_VIDEO);

  PHYSFS_init(argv[0]);

  ma_engine engine;
  auto engine_config = ma_engine_config_init();
  engine_config.channels = 2;
  engine_config.sampleRate = 48000;
  ma_engine_init(&engine_config, &engine);
  audioengine = &engine;

  L = luaL_newstate();
  luaL_openlibs(L);

  SteamAPI_InitSafe();

  application app;
  const auto result = app.run();

  SteamAPI_Shutdown();

  lua_close(L);

  ma_engine_uninit(&engine);

  PHYSFS_deinit();

  SDL_Quit();

  return result;
}
