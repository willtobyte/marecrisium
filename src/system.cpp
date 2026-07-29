void runtime::wire() {
  lua_getglobal(L, "platform");
  lua_pushliteral(L, "openurl");
  lua_pushcfunction(L, +[](lua_State* state) -> int {
    lua_pushboolean(state, SDL_OpenURL(luaL_checkstring(state, 2)));
    return 1;
  });
  lua_rawset(L, -3);
  lua_pop(L, 1);
}
