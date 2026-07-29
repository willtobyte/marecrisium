static int translate_callback(lua_State *state) {
  const auto extras = lua_gettop(state) - 1;

  lua_pushvalue(state, lua_upvalueindex(2));
  lua_pushvalue(state, lua_upvalueindex(1));
  lua_pushvalue(state, 1);
  lua_rawget(state, -2);
  lua_replace(state, -2);

  for (auto index = 0; index < extras; ++index)
    lua_pushvalue(state, 2 + index);

  lua_call(state, 1 + extras, 1);
  return 1;
}

void locales::wire() {
  const auto preferred = std::unique_ptr<SDL_Locale*[], SDL_Deleter>{SDL_GetPreferredLocales(nullptr)};
  const auto filename = std::format("locales/{}.lua", preferred[0]->language);
  if (!io::exists(filename)) [[unlikely]] {
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format");
    lua_remove(L, -2);
    lua_setglobal(L, "_");
    return;
  }

  const auto buffer = io::read(filename);
  const auto chunk = std::format("@{}", filename);
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(buffer.data()), buffer.size(), chunk.c_str()) != LUA_OK) [[unlikely]] {
    lua_error(L);
    std::unreachable();
  }

  lua_call(L, 0, 1);

  lua_getglobal(L, "string");
  lua_getfield(L, -1, "format");
  lua_remove(L, -2);

  lua_pushcclosure(L, translate_callback, 2);
  lua_setglobal(L, "_");
}
