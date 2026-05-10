namespace {
  static int openurl(lua_State *state) {
    const auto *url = luaL_checkstring(state, 1);
    lua_pushboolean(state, SDL_OpenURL(url) ? 1 : 0);
    return 1;
  }
}

void internet::wire() {
  lua_newtable(L);

  // lua_pushcfunction(L, connect);
  // lua_setfield(L, -2, "connect");

  // lua_pushcfunction(L, disconnect);
  // lua_setfield(L, -2, "disconnect");

  // lua_pushcfunction(L, watch);
  // lua_setfield(L, -2, "watch");

  // lua_pushcfunction(L, subscribe);
  // lua_setfield(L, -2, "subscribe");

  // lua_pushcfunction(L, unsubscribe);
  // lua_setfield(L, -2, "unsubscribe");

  // lua_newtable(L);
  // lua_pushcfunction(L, index);
  // lua_setfield(L, -2, "__index");
  // lua_setmetatable(L, -2);

  lua_setglobal(L, "internet");

  lua_pushcfunction(L, openurl);
  lua_setglobal(L, "openurl");
}

void internet::tick() {
}
