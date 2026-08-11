static int index(lua_State *state) {
  std::size_t length;
  const auto* data = luaL_checklstring(state, 2, &length);
  const std::string_view key{data, length};

  if (key == "shown") [[unlikely]] {
    lua_pushboolean(state, SDL_CursorVisible());
    return 1;
  }

  float x, y;
  const auto button = SDL_GetMouseState(&x, &y);
  SDL_RenderCoordinatesFromWindow(renderer, x, y, &x, &y);
  x += viewport.x;
  y += viewport.y;

  if (key == "x") {
    lua_pushnumber(state, static_cast<lua_Number>(x));
    return 1;
  }

  if (key == "y") {
    lua_pushnumber(state, static_cast<lua_Number>(y));
    return 1;
  }

  if (key == "left") {
    lua_pushboolean(state, !!(button & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)));
    return 1;
  }

  if (key == "middle") {
    lua_pushboolean(state, !!(button & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)));
    return 1;
  }

  if (key == "right") {
    lua_pushboolean(state, !!(button & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)));
    return 1;
  }

  return lua_pushnil(state), 1;
}

static int newindex(lua_State *state) {
  lua_toboolean(state, 3) ? SDL_ShowCursor() : SDL_HideCursor();
  return 0;
}

void mouse::wire() {
  luaL_newmetatable(L, "Mouse");
  lua_pushliteral(L, "Mouse");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pop(L, 1);

  lua_newuserdata(L, 1);
  luaL_getmetatable(L, "Mouse");
  lua_setmetatable(L, -2);
  lua_setglobal(L, "mouse");

  lua_pushliteral(L, "left");
  labels[0] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushliteral(L, "middle");
  labels[1] = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushliteral(L, "right");
  labels[2] = luaL_ref(L, LUA_REGISTRYINDEX);
}
