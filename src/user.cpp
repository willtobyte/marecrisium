void user::wire() {
  lua_newtable(L);

  const auto name = GetPersonaName();
  lua_pushstring(L, name);
  lua_setfield(L, -2, "persona");

  const auto count = GetFriendCount();
  lua_newtable(L);

  auto slot = 1;
  for (auto index = 0; index < count; ++index) {
    const auto id = GetFriendByIndex(index);
    if (id == 0) [[unlikely]]
      continue;

    const auto name = GetFriendPersonaName(id);

    lua_newtable(L);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    lua_setfield(L, -2, "id");
    lua_pushstring(L, name);
    lua_setfield(L, -2, "name");
    lua_rawseti(L, -2, slot++);
  }

  lua_setfield(L, -2, "friends");
  lua_setglobal(L, "user");
}
