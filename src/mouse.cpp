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
  const auto key = take<std::string_view>(state, 2);

  float x, y;
  const auto button = SDL_GetMouseState(&x, &y);
  SDL_RenderCoordinatesFromWindow(renderer, x, y, &x, &y);
  x += viewport.x;
  y += viewport.y;

  if (key == "x")
    return push(state, x);

  if (key == "y")
    return push(state, y);

  if (key == "position") {
    lua_pushcfunction(state, mouse_position);
    return 1;
  }

  if (key == "button") {
    if (button & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))
      return push(state, SDL_BUTTON_LEFT);
    if (button & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE))
      return push(state, SDL_BUTTON_MIDDLE);
    if (button & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT))
      return push(state, SDL_BUTTON_RIGHT);
    return push(state, 0);
  }

  if (key == "shown")
    return push(state, SDL_CursorVisible());

  return lua_pushnil(state), 1;
}

static int mouse_newindex(lua_State *state) {
  const auto key = take<std::string_view>(state, 2);

  if (key == "shown" && lua_isboolean(state, 3)) {
    if (lua_toboolean(state, 3))
      SDL_ShowCursor();
    else
      SDL_HideCursor();
  }

  return 0;
}

void mouse::wire() {
  metatable(L, "Mouse", mouse_index, mouse_newindex);

  singleton(L, "Mouse", "mouse");
}
