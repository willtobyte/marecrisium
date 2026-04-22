namespace {
  namespace property {
    constexpr auto id = "id"_hs;
    constexpr auto name = "name"_hs;
    constexpr auto persona = "persona"_hs;
    constexpr auto friends = "friends"_hs;
  }
}

static int _persona_ref = LUA_NOREF;
static int _friends_ref = LUA_NOREF;

static int friend_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  switch (id) {
    case property::id:
      lua_getfenv(state, 1);
      lua_getfield(state, -1, "id");
      lua_remove(state, -2);
      return 1;

    case property::name:
      lua_getfenv(state, 1);
      lua_getfield(state, -1, "name");
      lua_remove(state, -2);
      return 1;

    default:
      return lua_pushnil(state), 1;
  }
}

static int user_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  switch (id) {
    case property::persona:
      lua_rawgeti(state, LUA_REGISTRYINDEX, _persona_ref);
      return 1;

    case property::friends:
      lua_rawgeti(state, LUA_REGISTRYINDEX, _friends_ref);
      return 1;

    default:
      return lua_pushnil(state), 1;
  }
}

void user::wire() {
  metatable(L, "Friend", friend_index);

  lua_pushstring(L, GetPersonaName());
  _persona_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  const auto count = GetFriendCount();
  lua_newtable(L);

  auto index = 1;
  for (auto i = 0; i < count; ++i) {
    const auto fi = GetFriendByIndex(i);
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

    lua_rawseti(L, -2, index++);
  }

  _friends_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  metatable(L, "User", user_index);

  singleton(L, "User", "user");
}
