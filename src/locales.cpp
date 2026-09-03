static int translate_callback(lua_State *state) {
  const auto extras = lua_gettop(state) - 1;

  lua_pushvalue(state, lua_upvalueindex(2));
  lua_pushvalue(state, lua_upvalueindex(1));
  lua_pushvalue(state, 1);
  lua_rawget(state, -2);
  lua_replace(state, -2);

  for (auto index = 0; index < extras; ++index)
    lua_pushvalue(state, 2 + index);

  if (pcall(state, 1 + extras, 1) != LUA_OK) [[unlikely]]
    return lua_error(state);

  return 1;
}

void locales::wire() {
  auto count = 0;
  const auto preferred = std::unique_ptr<SDL_Locale*[], SDL_Deleter>{SDL_GetPreferredLocales(&count)};
  if (preferred && count > 0) [[likely]] {
    const auto chunk = std::format("@locales/{}.lua", preferred[0]->language);
    const auto path = std::string_view{chunk}.substr(1);
    if (const auto source = io::try_read(path)) [[likely]] {
      if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source->data()), source->size(), chunk.c_str()) != LUA_OK) [[unlikely]]
        throw std::runtime_error{lua_tostring(L, -1)};

      if (pcall(L, 0, 1) != LUA_OK) [[unlikely]]
        throw std::runtime_error{lua_tostring(L, -1)};

      lua_getglobal(L, "string");
      lua_getfield(L, -1, "format");
      lua_remove(L, -2);

      lua_pushcclosure(L, translate_callback, 2);
      lua_setglobal(L, "_");
      return;
    }
  }

  lua_getglobal(L, "string");
  lua_getfield(L, -1, "format");
  lua_remove(L, -2);
  lua_setglobal(L, "_");
}
