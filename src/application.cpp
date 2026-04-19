#include "application.hpp"

int application::run() {
  const auto* const rom = std::getenv("CARTRIDGE");
  filesystem::mount(rom ? rom : "cartridge.rom", "/");
  filesystem::try_mount("modifications", "/");
  filesystem::try_mount("modifications.zip", "/");

  scriptengine se;
  se.run();

  return 0;
}
