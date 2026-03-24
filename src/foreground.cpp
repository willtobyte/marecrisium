#include "foreground.hpp"

namespace {
int foreground_draw(lua_State *state) {
  auto *self = *static_cast<foreground **>(luaL_checkudata(state, 1, "Foreground"));
  luaL_checktype(state, 2, LUA_TTABLE);
  const auto count = static_cast<int>(luaL_checkinteger(state, 3));

  if (count <= 0 || count % 6 != 0) [[unlikely]]
    return 0;

  auto &vertices = self->_vertices;
  auto &indices = self->_indices;

  vertices.clear();
  indices.clear();

  const auto quads = count / 6;

  for (auto q = 0; q < quads; ++q) {
    const auto index = q * 6;

    lua_rawgeti(state, 2, index + 1);
    lua_rawgeti(state, 2, index + 2);
    lua_rawgeti(state, 2, index + 3);
    lua_rawgeti(state, 2, index + 4);
    lua_rawgeti(state, 2, index + 5);
    lua_rawgeti(state, 2, index + 6);

    const auto x = static_cast<float>(lua_tonumber(state, -6));
    const auto y = static_cast<float>(lua_tonumber(state, -5));
    const auto w = static_cast<float>(lua_tonumber(state, -4));
    const auto h = static_cast<float>(lua_tonumber(state, -3));
    const auto angle = static_cast<float>(lua_tonumber(state, -2));
    const auto alpha = static_cast<float>(lua_tonumber(state, -1)) / 255.f;

    lua_pop(state, 6);

    if (alpha <= .0f) [[unlikely]]
      continue;

    const SDL_FColor color{1.f, 1.f, 1.f, alpha};
    const auto base = static_cast<int32_t>(vertices.size());
    const auto hw = w * .5f;
    const auto hh = h * .5f;
    const auto cx = x + hw;
    const auto cy = y + hh;

    auto sa = .0f, ca = 1.f;
    if (angle != .0f)
      sincos(to_radians(angle), sa, ca);

    const auto dx0 = -hw * ca + hh * sa;
    const auto dy0 = -hw * sa - hh * ca;
    const auto dx1 = hw * ca + hh * sa;
    const auto dy1 = hw * sa - hh * ca;

    vertices.emplace_back(SDL_Vertex{{cx + dx0, cy + dy0}, color, {.0f, .0f}});
    vertices.emplace_back(SDL_Vertex{{cx + dx1, cy + dy1}, color, {1.f, .0f}});
    vertices.emplace_back(SDL_Vertex{{cx - dx0, cy - dy0}, color, {1.f, 1.f}});
    vertices.emplace_back(SDL_Vertex{{cx - dx1, cy - dy1}, color, {.0f, 1.f}});

    indices.emplace_back(base);
    indices.emplace_back(base + 1);
    indices.emplace_back(base + 2);
    indices.emplace_back(base);
    indices.emplace_back(base + 2);
    indices.emplace_back(base + 3);
  }

  if (vertices.empty()) [[unlikely]]
    return 0;

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture *>(*self->_texture),
    vertices.data(),
    static_cast<int>(vertices.size()),
    indices.data(),
    static_cast<int>(indices.size())
  );

  return 0;
}

int foreground_index(lua_State *state) {
  auto *self = *static_cast<foreground **>(luaL_checkudata(state, 1, "Foreground"));
  const auto key = std::string_view{luaL_checkstring(state, 2)};

  if (key == "draw") {
    lua_pushcfunction(state, foreground_draw);
    return 1;
  }

  return dispatch(state, self->_reference, key);
}
}

foreground::foreground(std::string_view name) {
  _vertices.reserve(2048);
  _indices.reserve(3072);

  auto &p = depot->pixmap.get(std::format("foregrounds/{}/pixmap", name));
  _texture = &p;

  const auto filename = std::format("foregrounds/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto label = std::format("@{}", filename);
  compile(L, buffer, label);

  pcall(L, 0, 1);

  lua_newtable(L);
  lua_pushnumber(L, static_cast<lua_Number>(static_cast<float>(p.width())));
  lua_setfield(L, -2, "width");
  lua_pushnumber(L, static_cast<lua_Number>(static_cast<float>(p.height())));
  lua_setfield(L, -2, "height");
  lua_setfield(L, -2, "pixmap");

  _reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

  lua_getfield(L, -1, "on_appear");
  _on_appear = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_disappear");
  _on_disappear = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_loop");
  _on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_paint");
  _on_paint = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);

  auto **m = static_cast<foreground **>(lua_newuserdata(L, sizeof(foreground *)));
  *m = this;
  luaL_getmetatable(L, "Foreground");
  lua_setmetatable(L, -2);
  _userdata_reference = luaL_ref(L, LUA_REGISTRYINDEX);
}

foreground::~foreground() {
  disappear();
  luaL_unref(L, LUA_REGISTRYINDEX, _on_paint);
  _on_paint = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  _on_loop = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, _on_disappear);
  _on_disappear = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, _on_appear);
  _on_appear = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, _reference);
  _reference = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, _userdata_reference);
  _userdata_reference = LUA_NOREF;
}

void foreground::wire() {
  metatable(L, "Foreground", foreground_index);
}

void foreground::expose() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata_reference);
  lua_setglobal(L, "foreground");
}

void foreground::appear() {
  if (_on_appear != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_appear);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    pcall(L, 1, 0);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, _on_appear);
  _on_appear = LUA_NOREF;
}

void foreground::disappear() {
  if (_on_disappear != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_disappear);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    pcall(L, 1, 0);
  }
  luaL_unref(L, LUA_REGISTRYINDEX, _on_disappear);
  _on_disappear = LUA_NOREF;
}

void foreground::update(float delta) {
  if (_on_loop != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    pcall(L, 2, 0);
  }
}

void foreground::draw() {
  if (_on_paint != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_paint);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata_reference);
    pcall(L, 1, 0);
  }
}
