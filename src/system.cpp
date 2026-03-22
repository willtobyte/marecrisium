#include "system.hpp"

static int runtime_moment(lua_State *state) {
  return push(state, static_cast<lua_Number>(SDL_GetTicks()));
}

void runtime::wire() {
  lua_pushcfunction(L, runtime_moment);
  lua_setglobal(L, "moment");
}
