static int unlock_callback(lua_State *state) {
  const auto *id = luaL_checkstring(state, 2);

  if (!SteamUserStats()) [[unlikely]] {
    lua_pushboolean(state, 0);
    return 1;
  }

  if (GetAchievement(id)) {
    lua_pushboolean(state, 1);
    return 1;
  }

  const auto result = SetAchievement(id);
  StoreStats();

  lua_pushboolean(state, result);
  return 1;
}

void achievement::wire() {
  lua_newtable(L);
  lua_pushcfunction(L, unlock_callback);
  lua_setfield(L, -2, "unlock");
  lua_setglobal(L, "achievement");
}
