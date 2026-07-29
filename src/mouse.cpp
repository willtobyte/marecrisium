namespace {
namespace lookup {
  constexpr auto x = "x"_hs;
  constexpr auto y = "y"_hs;
  constexpr auto left = "left"_hs;
  constexpr auto middle = "middle"_hs;
  constexpr auto right = "right"_hs;
  constexpr auto shown = "shown"_hs;
}
}

static int index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == lookup::shown) {
    lua_pushboolean(state, SDL_CursorVisible() ? 1 : 0);
    return 1;
  }

  float x, y;
  const auto button = SDL_GetMouseState(&x, &y);
  SDL_RenderCoordinatesFromWindow(renderer, x, y, &x, &y);
  x += viewport.x;
  y += viewport.y;

  switch (id) {
    case lookup::x:
      lua_pushnumber(state, static_cast<lua_Number>(x));
      return 1;

    case lookup::y:
      lua_pushnumber(state, static_cast<lua_Number>(y));
      return 1;

    case lookup::left:
      lua_pushboolean(state, !!(button & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)));
      return 1;

    case lookup::middle:
      lua_pushboolean(state, !!(button & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)));
      return 1;

    case lookup::right:
      lua_pushboolean(state, !!(button & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)));
      return 1;

    default:
      return lua_pushnil(state), 1;
  }
}

static int newindex(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id != lookup::shown || !lua_isboolean(state, 3))
    return 0;

  lua_toboolean(state, 3)
    ? SDL_ShowCursor()
    : SDL_HideCursor();

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
}
