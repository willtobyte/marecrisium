void platform::wire() {
  lua_newtable(L);
  lua_pushstring(L, SDL_GetPlatform());
  lua_setfield(L, -2, "name");
  lua_pushinteger(L, static_cast<lua_Integer>(SDL_GetNumLogicalCPUCores()));
  lua_setfield(L, -2, "cores");
  lua_pushinteger(L, static_cast<lua_Integer>(SDL_GetSystemRAM()));
  lua_setfield(L, -2, "memory");
  lua_setglobal(L, "platform");
}
