#include "graphics.hpp"

static int draw(lua_State* state) {
  auto* pixmappool = static_cast<::pixmappool*>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto* name = luaL_checkstring(state, 1);
  const auto* vertices = static_cast<const SDL_Vertex*>(lua_topointer(state, 2));
  const auto vertice_count = static_cast<int>(luaL_checkinteger(state, 3));
  const auto* indices = static_cast<const int*>(lua_topointer(state, 4));
  const auto index_count = static_cast<int>(luaL_checkinteger(state, 5));

  const auto& pixmap = pixmappool->get(name);

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(pixmap),
    vertices,
    vertice_count,
    indices,
    index_count
  );

  return 0;
}

namespace graphics {
void wire(pixmappool* pixmappool) {
  lua_newtable(L);

  lua_pushlightuserdata(L, pixmappool);
  lua_pushcclosure(L, draw, 1);
  lua_setfield(L, -2, "draw");

  lua_setglobal(L, "graphics");
}
}
