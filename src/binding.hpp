#pragma once

#include "common.hpp"

void pcall(lua_State *state, int nargs, int nresults);
void fcall(lua_State *state, int nargs, int nresults) noexcept;
void compile(lua_State *state, std::span<const uint8_t> buffer, std::string_view chunk);
void metatable(lua_State *state, const char *name, lua_CFunction index, lua_CFunction newindex = nullptr, lua_CFunction gc = nullptr) noexcept;
void singleton(lua_State *state, const char *metatable, const char *global) noexcept;

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
