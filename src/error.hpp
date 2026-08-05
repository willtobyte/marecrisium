#pragma once

namespace error {
inline void check(lua_State* state, int status) {
  if (status == LUA_OK) [[likely]]
    return;

  lua_error(state);
  std::unreachable();
}

[[noreturn]] void raise(lua_State* state);
}
