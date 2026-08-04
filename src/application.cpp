int application::run() {
  try {
    io::mount("cartridge.rom");

    scriptengine se;
    se.run();
  } catch (const std::exception& exc) {
    const auto message = exc.what();

    std::fputs(message, stderr);
    std::fputc('\n', stderr);

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Stampageddon", message, nullptr);

    return 1;
  }

  return 0;
}
