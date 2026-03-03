#include "application.hpp"

int application::run() {
  try {
    const auto* const rom = std::getenv("CARTRIDGE");
    filesystem::mount(rom ? rom : "cartridge.rom", "/");

    auto se = scriptengine();
    se.run();
  } catch (const std::exception& exc) {
    const auto* const error = exc.what();

    std::println(stderr, "{}", error);

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Ink Spill Disaster", error, nullptr);

    return 1;
  }

  return 0;
}
