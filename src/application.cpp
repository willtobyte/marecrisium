int application::run() {
  try {
    const auto* const rom = std::getenv("CARTRIDGE");
    filesystem::mount(rom ? rom : "cartridge.rom", "/");
    filesystem::try_mount("modifications", "/");
    filesystem::try_mount("modifications.zip", "/");

    scriptengine se;
    se.run();
  } catch (const std::exception& exc) {
    const auto message = std::format("{}: {}", typeid(exc).name(), exc.what());

    std::println(stderr, "{}", message);

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Ink Spill Disaster", message.c_str(), nullptr);

    return 1;
  }

  return 0;
}
