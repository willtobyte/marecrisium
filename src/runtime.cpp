static int index(lua_State *state) {
  if (!lua_rawequal(state, 2, lua_upvalueindex(1)))
    return 0;

  const auto text = std::unique_ptr<char, SDL_Deleter>{SDL_GetClipboardText()};
  lua_pushstring(state, text.get());
  return 1;
}

static int newindex(lua_State *state) {
  if (lua_rawequal(state, 2, lua_upvalueindex(1))) {
    SDL_SetClipboardText(luaL_checkstring(state, 3));
    return 0;
  }

  lua_rawset(state, 1);
  return 0;
}

void runtime::wire() {
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_newtable(L);
  lua_pushliteral(L, "clipboard");
  lua_pushcclosure(L, index, 1);
  lua_setfield(L, -2, "__index");
  lua_pushliteral(L, "clipboard");
  lua_pushcclosure(L, newindex, 1);
  lua_setfield(L, -2, "__newindex");
  lua_setmetatable(L, -2);
  lua_pop(L, 1);

  lua_pushcfunction(L, +[](lua_State* state) -> int {
    lua_pushnumber(state, static_cast<lua_Number>(SDL_GetTicks()));
    return 1;
  });

  lua_setglobal(L, "moment");

  lua_pushcfunction(L, +[](lua_State* state) -> int {
    lua_pushboolean(state, SDL_OpenURL(luaL_checkstring(state, 1)));
    return 1;
  });

  lua_setglobal(L, "openurl");
}
