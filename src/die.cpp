#include "die.hpp"

[[noreturn]] void die(std::string message) noexcept {
#ifndef DEBUG
  const auto event = sentry_value_new_event();
  const auto exception = sentry_value_new_exception("fatal", message.c_str());
  sentry_value_set_stacktrace(exception, nullptr, 0);
  sentry_event_add_exception(event, exception);
  sentry_capture_event(event);
  sentry_flush(3000);
#endif

  std::println(stderr, "{}", message);

  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Ink Spill Disaster", message.c_str(), nullptr);

  std::_Exit(1);
}

[[noreturn]] void die(lua_State *state) noexcept {
  const auto *message = lua_tostring(state, -1);
  die(message ? message : "unknown lua error");
}
