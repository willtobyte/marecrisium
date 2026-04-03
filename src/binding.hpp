#pragma once

#include "common.hpp"

void pcall(lua_State *state, int nargs, int nresults);
void compile(lua_State *state, const std::vector<uint8_t> &buffer, std::string_view chunk);
void metatable(lua_State *state, const char *name, lua_CFunction index, lua_CFunction newindex = nullptr, lua_CFunction gc = nullptr) noexcept;
void singleton(lua_State *state, const char *metatable, const char *global) noexcept;
