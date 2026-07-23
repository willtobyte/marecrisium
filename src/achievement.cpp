namespace {
  namespace lookup {
    constexpr auto unlock = "unlock"_hs;
  }
}

static int achievement_unlock(lua_State *state) {
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

static int _unlock_reference = LUA_NOREF;

static int achievement_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == lookup::unlock)
    return lua_rawgeti(state, LUA_REGISTRYINDEX, _unlock_reference), 1;

  return lua_pushnil(state), 1;
}

void achievement::wire() {
  binding::callback(L, achievement_unlock);
  _unlock_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  binding::metatable(L, "Achievement", achievement_index);

  binding::singleton(L, "Achievement", "achievement");
}
