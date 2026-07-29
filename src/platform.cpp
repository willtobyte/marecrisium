namespace {
  namespace lookup {
    constexpr auto name = "name"_hs;
    constexpr auto cores = "cores"_hs;
    constexpr auto memory = "memory"_hs;
    constexpr auto clipboard = "clipboard"_hs;
  }
}

static int index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  switch (id) {
    case lookup::name:
      lua_pushstring(state, SDL_GetPlatform());
      return 1;

    case lookup::cores:
      lua_pushinteger(state, static_cast<lua_Integer>(SDL_GetNumLogicalCPUCores()));
      return 1;

    case lookup::memory:
      lua_pushinteger(state, static_cast<lua_Integer>(SDL_GetSystemRAM()));
      return 1;

    case lookup::clipboard: {
      const auto text = std::unique_ptr<char, SDL_Deleter>{SDL_GetClipboardText()};
      lua_pushstring(state, text ? text.get() : "");
      return 1;
    }

    default:
      return lua_pushnil(state), 1;
  }
}

static int newindex(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == lookup::clipboard) {
    const auto *text = luaL_checkstring(state, 3);
    SDL_SetClipboardText(text);
  }

  return 0;
}

void platform::wire() {
  luaL_newmetatable(L, "Platform");
  lua_pushliteral(L, "Platform");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pop(L, 1);

  lua_newuserdata(L, 1);
  luaL_getmetatable(L, "Platform");
  lua_setmetatable(L, -2);
  lua_setglobal(L, "platform");
}
