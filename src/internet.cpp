#include "internet.hpp"

static int openurl_call(lua_State *state) {
  const auto *url = luaL_checkstring(state, 1);
  lua_pushboolean(state, SDL_OpenURL(url) ? 1 : 0);
  return 1;
}

void internet::wire() {
  lua_pushcfunction(L, openurl_call);
  lua_setglobal(L, "openurl");
}
