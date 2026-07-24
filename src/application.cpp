int application::run() {
  try {
    io::mount("cartridge.rom");

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
