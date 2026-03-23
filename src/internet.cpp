#include "internet.hpp"

static int openurl_call(lua_State *state) {
  const auto *url = check<const char *>(state, 1);
  lua_pushboolean(state, SDL_OpenURL(url));
  return 1;
}

void internet::wire() {
  lua_pushcfunction(L, openurl_call);
  lua_setglobal(L, "openurl");
}
