#pragma once

#include "common.hpp"

void pcall(lua_State *state, int nargs, int nresults);
void hcall(lua_State *state, int nargs, int nresults, int handler);
void fcall(lua_State *state, int nargs, int nresults);
[[nodiscard]] int push_traceback(lua_State *state);
void compile(lua_State *state, std::span<const uint8_t> buffer, std::string_view chunk);
void metatable(lua_State *state, const char *name, lua_CFunction index, lua_CFunction newindex = nullptr, lua_CFunction gc = nullptr);
void singleton(lua_State *state, const char *metatable, const char *global);

template <lua_CFunction F>
int guard(lua_State *state) {
  try {
    return F(state);
  } catch (const std::exception &exc) {
    return luaL_error(state, "%s", exc.what());
  }
}

namespace binding {
void wire();
}
