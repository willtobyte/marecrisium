#pragma once

#include "common.hpp"

#ifdef _MSC_VER
#define BINDING_HIDDEN
#else
#define BINDING_HIDDEN __attribute__((visibility("hidden")))
#endif

namespace binding {
enum class fault { fatal, ignore };

BINDING_HIDDEN bool call(lua_State *state, int arguments, int results);
BINDING_HIDDEN bool call(lua_State *state, int arguments, int results, fault mode);
BINDING_HIDDEN void load(lua_State *state, std::span<const uint8_t> buffer, const char *chunk);
inline void load(lua_State *state, std::span<const uint8_t> buffer, const std::string &chunk) {
  load(state, buffer, chunk.c_str());
}

BINDING_HIDDEN void metatable(lua_State *state, const char *name, lua_CFunction index, lua_CFunction newindex = nullptr, lua_CFunction gc = nullptr);
BINDING_HIDDEN void singleton(lua_State *state, const char *metatable, const char *global);
BINDING_HIDDEN void callback(lua_State *state, lua_CFunction native);
BINDING_HIDDEN void closure(lua_State *state, lua_CFunction native, int upvalues);
}

#undef BINDING_HIDDEN
