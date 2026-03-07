#include "atlas.hpp"

namespace {
int atlas_draw(lua_State* state) {
  const auto* const* ptr = static_cast<const pixmap* const*>(luaL_checkudata(state, 1, "Atlas"));
  const auto* vertices = static_cast<const SDL_Vertex*>(lua_topointer(state, 2));
  const auto vertex_count = static_cast<int>(luaL_checkinteger(state, 3));
  const auto* indices = static_cast<const int*>(lua_topointer(state, 4));
  const auto index_count = static_cast<int>(luaL_checkinteger(state, 5));

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(**ptr),
    vertices,
    vertex_count,
    indices,
    index_count
  );

  return 0;
}

int atlas_index(lua_State* state) {
  const auto* const* ptr = static_cast<const pixmap* const*>(luaL_checkudata(state, 1, "Atlas"));
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "draw") {
    lua_pushcfunction(state, atlas_draw);
    return 1;
  }

  if (key == "width") {
    lua_pushinteger(state, (*ptr)->width());
    return 1;
  }

  if (key == "height") {
    lua_pushinteger(state, (*ptr)->height());
    return 1;
  }

  return lua_pushnil(state), 1;
}
}

namespace atlas {
void wire() {
  luaL_newmetatable(L, "Atlas");

  lua_pushcfunction(L, atlas_index);
  lua_setfield(L, -2, "__index");

  lua_pop(L, 1);
}
}
