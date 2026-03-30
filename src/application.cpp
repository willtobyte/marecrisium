#include "application.hpp"

int application::run() {
  try {
    const auto* const rom = std::getenv("CARTRIDGE");
    filesystem::mount(rom ? rom : "cartridge.rom", "/");
    filesystem::try_mount("modifications", "/");
    filesystem::try_mount("modifications.zip", "/");

    auto se = scriptengine();
    se.run();
  } catch (const std::exception& exc) {
    const auto* const error = exc.what();

#ifndef DEBUG
    const auto event = sentry_value_new_event();
    const auto exception = sentry_value_new_exception("exception", error);
    sentry_value_set_stacktrace(exception, nullptr, 0);
    sentry_event_add_exception(event, exception);
    sentry_capture_event(event);
    sentry_flush(3000);
#endif

    std::println(stderr, "{}", error);

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Ink Spill Disaster", error, nullptr);

    return 1;
  }

  return 0;
}
