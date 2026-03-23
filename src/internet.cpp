#include "internet.hpp"

static int openurl_call(lua_State *state) {
  const auto *url = argument<const char *>(state, 1);
  push(state, SDL_OpenURL(url));
  return 1;
}

void internet::wire() {
  lua_pushcfunction(L, openurl_call);
  lua_setglobal(L, "openurl");
}
