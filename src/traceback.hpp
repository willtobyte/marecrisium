#pragma once

namespace traceback {
// luaL_ref allocates positive slots; -1 and -2 are its sentinels.
inline constexpr int slot{-3};

int build(lua_State* state);
}
