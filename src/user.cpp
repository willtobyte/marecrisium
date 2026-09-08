void user::wire() {
  lua_createtable(L, 0, 2);

  const auto name = GetPersonaName();
  lua_pushstring(L, name);
  lua_setfield(L, -2, "persona");

  const auto count = GetFriendCount();
  lua_createtable(L, count, 0);

  for (auto index = 0; index < count; ++index) {
    const auto id = GetFriendByIndex(index);
    const auto name = GetFriendPersonaName(id);

    lua_createtable(L, 0, 2);
    lua_pushinteger(L, static_cast<lua_Integer>(id & 0xFFFFFFFF));
    lua_setfield(L, -2, "id");
    lua_pushstring(L, name);
    lua_setfield(L, -2, "name");
    lua_rawseti(L, -2, index + 1);
  }

  lua_setfield(L, -2, "friends");
  lua_setglobal(L, "user");
}
