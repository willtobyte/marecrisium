namespace {
keyring<
  "snapshot",
  "shown"
> keys;

static int snapshot(lua_State* state) {
  float x, y;
  const auto button = SDL_GetMouseState(&x, &y);

  SDL_RenderCoordinatesFromWindow(renderer, x, y, &x, &y);

  lua_pushnumber(state, static_cast<lua_Number>(x));
  lua_pushnumber(state, static_cast<lua_Number>(y));
  lua_pushboolean(state, !!(button & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)));
  // lua_pushboolean(state, !!(button & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)));
  // lua_pushboolean(state, !!(button & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)));

  return 3;
}

static int index(lua_State *state) {
  switch (keys.find(lua_tostring(state, 2))) {
  case keys.id<"snapshot">():
    lua_pushvalue(state, lua_upvalueindex(1));

    return 1;

  case keys.id<"shown">():
    lua_pushboolean(state, SDL_CursorVisible());

    return 1;

  default:
    return lua_pushnil(state), 1;
  }
}

static int newindex(lua_State *state) {
  lua_toboolean(state, 3) ? SDL_ShowCursor() : SDL_HideCursor();

  return 0;
}

}

void mouse::wire() {
  keys.intern();

  lua_createtable(L, 0, 3);
  lua_pushliteral(L, "Mouse");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, snapshot);
  lua_pushcclosure(L, index, 1);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");

  lua_newuserdata(L, 1);
  lua_pushvalue(L, -2);
  lua_setmetatable(L, -2);
  lua_setglobal(L, "mouse");
  lua_setfield(L, LUA_REGISTRYINDEX, "Mouse");
}
