static int index(lua_State *state) {
  auto *text = SDL_GetClipboardText();
  lua_pushstring(state, text);
  SDL_free(text);
  return 1;
}

static int newindex(lua_State *state) {
  SDL_SetClipboardText(luaL_checkstring(state, 3));
  return 0;
}

void platform::wire() {
  lua_newtable(L);
  lua_pushstring(L, SDL_GetPlatform());
  lua_setfield(L, -2, "name");
  lua_pushinteger(L, static_cast<lua_Integer>(SDL_GetNumLogicalCPUCores()));
  lua_setfield(L, -2, "cores");
  lua_pushinteger(L, static_cast<lua_Integer>(SDL_GetSystemRAM()));
  lua_setfield(L, -2, "memory");

  lua_newtable(L);
  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_setmetatable(L, -2);
  lua_setglobal(L, "platform");
}
