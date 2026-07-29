void user::wire() {
  lua_newtable(L);
  lua_pushstring(L, GetPersonaName());
  lua_setfield(L, -2, "persona");

  const auto count = GetFriendCount();
  lua_newtable(L);

  auto slot = 1;
  for (auto index = 0; index < count; ++index) {
    const auto id = GetFriendByIndex(index);
    if (id == 0) [[unlikely]]
      continue;

    const std::string_view name = GetFriendPersonaName(id);
    if (name.empty()) [[unlikely]]
      continue;

    lua_newtable(L);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    lua_setfield(L, -2, "id");
    lua_pushlstring(L, name.data(), name.size());
    lua_setfield(L, -2, "name");
    lua_rawseti(L, -2, slot++);
  }

  lua_setfield(L, -2, "friends");
  lua_setglobal(L, "user");
}
