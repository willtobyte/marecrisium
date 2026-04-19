#pragma once

[[noreturn]] void die(std::string message) noexcept;
[[noreturn]] void die(lua_State *state) noexcept;

template <typename... Args>
[[noreturn]] void die(std::format_string<Args...> fmt, Args&&... args) noexcept {
  die(std::format(fmt, std::forward<Args>(args)...));
}
