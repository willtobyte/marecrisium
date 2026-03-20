#include "mouse.hpp"

static int mouse_position(lua_State *state) {
  float x, y;
  SDL_GetMouseState(&x, &y);
  SDL_RenderCoordinatesFromWindow(renderer, x, y, &x, &y);
  x += viewport.x;
  y += viewport.y;
  lua_pushnumber(state, static_cast<double>(x));
  lua_pushnumber(state, static_cast<double>(y));
  return 2;
}

static int mouse_index(lua_State *state) {
  const std::string_view key = luaL_checkstring(state, 2);

  float x, y;
  const auto button = SDL_GetMouseState(&x, &y);
  SDL_RenderCoordinatesFromWindow(renderer, x, y, &x, &y);
  x += viewport.x;
  y += viewport.y;

  if (key == "x") {
    lua_pushnumber(state, static_cast<double>(x));
    return 1;
  }

  if (key == "y") {
    lua_pushnumber(state, static_cast<double>(y));
    return 1;
  }

  if (key == "position") {
    lua_pushcfunction(state, mouse_position);
    return 1;
  }

  if (key == "button") {
    if (button & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))
      return lua_pushinteger(state, SDL_BUTTON_LEFT), 1;
    if (button & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE))
      return lua_pushinteger(state, SDL_BUTTON_MIDDLE), 1;
    if (button & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))
      return lua_pushinteger(state, SDL_BUTTON_RIGHT), 1;
    lua_pushinteger(state, 0);
    return 1;
  }

  if (key == "shown") {
    lua_pushboolean(state, SDL_CursorVisible());
    return 1;
  }

  lua_pushnil(state);
  return 1;
}

static int mouse_newindex(lua_State *state) {
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "shown" && lua_isboolean(state, 3)) {
    if (lua_toboolean(state, 3))
      SDL_ShowCursor();
    else
      SDL_HideCursor();
  }

  return 0;
}

void mouse::wire() {
  lua_newuserdata(L, 1);

  luaL_newmetatable(L, "Mouse");
  lua_pushcfunction(L, mouse_index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, mouse_newindex);
  lua_setfield(L, -2, "__newindex");

  lua_setmetatable(L, -2);
  lua_setglobal(L, "mouse");
}
