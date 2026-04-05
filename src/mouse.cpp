#include "mouse.hpp"

namespace {
namespace property {
  constexpr auto x      = "x"_hs;
  constexpr auto y      = "y"_hs;
  constexpr auto button = "button"_hs;
  constexpr auto shown  = "shown"_hs;
}
}

static int mouse_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == property::shown) {
    lua_pushboolean(state, SDL_CursorVisible() ? 1 : 0);
    return 1;
  }

  float x, y;
  const auto button = SDL_GetMouseState(&x, &y);
  SDL_RenderCoordinatesFromWindow(renderer, x, y, &x, &y);
  x += viewport.x;
  y += viewport.y;

  switch (id) {
    case property::x:
      lua_pushnumber(state, static_cast<lua_Number>(x));
      return 1;

    case property::y:
      lua_pushnumber(state, static_cast<lua_Number>(y));
      return 1;

    case property::button:
      if (button & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) {
        lua_pushinteger(state, static_cast<lua_Integer>(SDL_BUTTON_LEFT));
        return 1;
      }

      if (button & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)) {
        lua_pushinteger(state, static_cast<lua_Integer>(SDL_BUTTON_MIDDLE));
        return 1;
      }

      if (button & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) {
        lua_pushinteger(state, static_cast<lua_Integer>(SDL_BUTTON_RIGHT));
        return 1;
      }

      lua_pushinteger(state, static_cast<lua_Integer>(0));
      return 1;

    default:
      return lua_pushnil(state), 1;
  }
}

static int mouse_newindex(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == property::shown && lua_isboolean(state, 3)) {
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
