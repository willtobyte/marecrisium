static int translate(lua_State *state) {
  const auto extras = lua_gettop(state) - 1;

  lua_pushvalue(state, lua_upvalueindex(2));
  lua_pushvalue(state, lua_upvalueindex(1));
  lua_pushvalue(state, 1);
  lua_rawget(state, -2);
  lua_replace(state, -2);

  if (lua_isnil(state, -1)) [[unlikely]] {
    lua_pop(state, 1);
    lua_pushvalue(state, 1);
  }

  for (auto i = 0; i < extras; ++i)
    lua_pushvalue(state, 2 + i);

  if (!pcall(state, 1 + extras, 1, fault::ignore)) [[unlikely]]
    lua_pushvalue(state, 1);

  return 1;
}

void locales::wire() {
  lua_newtable(L);

  auto count = 0;
  const auto preferred = std::unique_ptr<SDL_Locale*[], SDL_Deleter>{SDL_GetPreferredLocales(&count)};
  if (preferred && count > 0) [[likely]] {
    const auto filename = std::format("locales/{}.lua", preferred[0]->language);

    if (io::exists(filename)) [[likely]] {
      compile(L, io::read(filename), std::format("@{}", filename));
      if (pcall(L, 0, 1, fault::ignore)) [[likely]]
        lua_replace(L, -2);
    }
  }

  lua_getglobal(L, "string");
  lua_getfield(L, -1, "format");
  lua_remove(L, -2);

  cclosure(L, translate, 2);
  lua_setglobal(L, "_");
}
