int application::run() {
  try {
    io::mount("cartridge.rom");

    scriptengine se;
    se.run();
  } catch (const std::exception& exception) {
    const auto message = exception.what();

    std::fputs(message, stderr);
    std::fputc('\n', stderr);

    const auto event = sentry_value_new_event();
    const auto error = sentry_value_new_exception("exception", message);
    sentry_value_set_stacktrace(error, nullptr, 0);
    sentry_event_add_exception(event, error);
    sentry_capture_event(event);
    sentry_flush(3000);

#ifndef DEBUG
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Stampageddon", message, nullptr);
#endif

    return 1;
  }

  return 0;
}
