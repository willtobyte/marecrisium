namespace {
  namespace lookup {
    constexpr auto unlock = "unlock"_hs;
  }
}

static int grant(lua_State *state) {
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

  lua_pushboolean(state, result ? 1 : 0);
  return 1;
}

static int unlock = LUA_NOREF;

static int index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == lookup::unlock)
    return lua_rawgeti(state, LUA_REGISTRYINDEX, unlock), 1;

  return lua_pushnil(state), 1;
}

void achievement::wire() {
  lua_pushcfunction(L, grant);
  unlock = luaL_ref(L, LUA_REGISTRYINDEX);

  luaL_newmetatable(L, "Achievement");
  lua_pushliteral(L, "Achievement");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);

  lua_newuserdata(L, 1);
  luaL_getmetatable(L, "Achievement");
  lua_setmetatable(L, -2);
  lua_setglobal(L, "achievement");
}
