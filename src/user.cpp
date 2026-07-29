namespace {
  namespace lookup {
    constexpr auto id = "id"_hs;
    constexpr auto name = "name"_hs;
    constexpr auto persona = "persona"_hs;
    constexpr auto friends = "friends"_hs;
  }

  int persona = LUA_NOREF;
  int friends = LUA_NOREF;
}

static int friend_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  switch (id) {
    case lookup::id:
      lua_getfenv(state, 1);
      lua_getfield(state, -1, "id");
      lua_remove(state, -2);
      return 1;

    case lookup::name:
      lua_getfenv(state, 1);
      lua_getfield(state, -1, "name");
      lua_remove(state, -2);
      return 1;

    default:
      return lua_pushnil(state), 1;
  }
}

static int index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  switch (id) {
    case lookup::persona:
      lua_rawgeti(state, LUA_REGISTRYINDEX, persona);
      return 1;

    case lookup::friends:
      lua_rawgeti(state, LUA_REGISTRYINDEX, friends);
      return 1;

    default:
      return lua_pushnil(state), 1;
  }
}

void user::wire() {
  luaL_newmetatable(L, "Friend");
  lua_pushliteral(L, "Friend");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, friend_index);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);

  lua_pushstring(L, GetPersonaName());
  persona = luaL_ref(L, LUA_REGISTRYINDEX);

  const auto count = GetFriendCount();
  lua_newtable(L);

  auto slot = 1;
  for (auto index = 0; index < count; ++index) {
    const auto fi = GetFriendByIndex(index);
    if (fi == 0) [[unlikely]]
      continue;

    const std::string_view name = GetFriendPersonaName(fi);
    if (name.empty()) [[unlikely]]
      continue;

    lua_newuserdata(L, 1);
    luaL_getmetatable(L, "Friend");
    lua_setmetatable(L, -2);

    lua_newtable(L);
    lua_pushinteger(L, static_cast<lua_Integer>(fi));
    lua_setfield(L, -2, "id");
    lua_pushlstring(L, name.data(), name.size());
    lua_setfield(L, -2, "name");
    lua_setfenv(L, -2);

    lua_rawseti(L, -2, slot++);
  }

  friends = luaL_ref(L, LUA_REGISTRYINDEX);

  luaL_newmetatable(L, "User");
  lua_pushliteral(L, "User");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);

  lua_newuserdata(L, 1);
  luaL_getmetatable(L, "User");
  lua_setmetatable(L, -2);
  lua_setglobal(L, "user");
}
