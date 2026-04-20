#pragma once

#include "common.hpp"

enum class fault { fatal, ignore };

bool pcall(lua_State *state, int nargs, int nresults, fault mode = fault::fatal);
void compile(lua_State *state, std::span<const uint8_t> buffer, std::string_view chunk);
void metatable(lua_State *state, const char *name, lua_CFunction index, lua_CFunction newindex = nullptr, lua_CFunction gc = nullptr);
void singleton(lua_State *state, const char *metatable, const char *global);

namespace binding {
void wire();
}
