void user::wire() {
  lua_newtable(L);

  const auto name = GetPersonaName();
  lua_pushstring(L, name);
  lua_setfield(L, -2, "persona");

  const auto count = GetFriendCount();
  lua_newtable(L);

  for (auto index = 0; index < count; ++index) {
    const auto id = GetFriendByIndex(index);
    assert((id >> 32) == 0x01100001 && "friend must be an individual account in the public universe");

    const auto name = GetFriendPersonaName(id);

    lua_newtable(L);
    lua_pushinteger(L, static_cast<lua_Integer>(id & 0xFFFFFFFF));
    lua_setfield(L, -2, "id");
    lua_pushstring(L, name);
    lua_setfield(L, -2, "name");
    lua_rawseti(L, -2, index + 1);
  }

  lua_setfield(L, -2, "friends");
  lua_setglobal(L, "user");
}
