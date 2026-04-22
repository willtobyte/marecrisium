namespace {
  namespace property {
    constexpr auto name = "name"_hs;
    constexpr auto cores = "cores"_hs;
    constexpr auto memory = "memory"_hs;
    constexpr auto clipboard = "clipboard"_hs;
  }
}

static int platform_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  switch (id) {
    case property::name:
      lua_pushstring(state, SDL_GetPlatform());
      return 1;

    case property::cores:
      lua_pushinteger(state, static_cast<lua_Integer>(SDL_GetNumLogicalCPUCores()));
      return 1;

    case property::memory:
      lua_pushinteger(state, static_cast<lua_Integer>(SDL_GetSystemRAM()));
      return 1;

    case property::clipboard: {
      const auto text = std::unique_ptr<char, SDL_Deleter>{SDL_GetClipboardText()};
      lua_pushstring(state, text ? text.get() : "");
      return 1;
    }

    default:
      return lua_pushnil(state), 1;
  }
}

static int platform_newindex(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == property::clipboard) {
    const auto *text = luaL_checkstring(state, 3);
    SDL_SetClipboardText(text);
  }

  return 0;
}

void platform::wire() {
  metatable(L, "Platform", platform_index, platform_newindex);

  singleton(L, "Platform", "platform");
}
