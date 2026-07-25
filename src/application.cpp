int application::run() {
  try {
    io::mount("cartridge.rom");

    scriptengine se;
    se.run();
  } catch (const std::exception& exc) {
    const auto message = exc.what();

    std::println(stderr, "{}", message);

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Ink Spill Disaster", message, nullptr);

    return 1;
  }

  return 0;
}
