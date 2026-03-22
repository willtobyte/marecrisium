#include "foreground.hpp"

namespace {
constexpr int STRIDE = 5;

int foreground_draw(lua_State *state) {
  auto *self = checkuserdata<foreground>(state, 1, "Foreground");
  luaL_checktype(state, 2, LUA_TTABLE);
  const auto count = static_cast<int>(luaL_checknumber(state, 3));

  if (count <= 0 || count % STRIDE != 0) [[unlikely]]
    return 0;

  auto &vertices = self->_vertices;
  auto &indices = self->_indices;

  vertices.clear();
  indices.clear();

  const auto quads = count / STRIDE;

  for (auto q = 0; q < quads; ++q) {
    const auto base_idx = q * STRIDE;

    lua_rawgeti(state, 2, base_idx + 1);
    lua_rawgeti(state, 2, base_idx + 2);
    lua_rawgeti(state, 2, base_idx + 3);
    lua_rawgeti(state, 2, base_idx + 4);
    lua_rawgeti(state, 2, base_idx + 5);

    const auto x = static_cast<float>(lua_tonumber(state, -5));
    const auto y = static_cast<float>(lua_tonumber(state, -4));
    const auto w = static_cast<float>(lua_tonumber(state, -3));
    const auto h = static_cast<float>(lua_tonumber(state, -2));
    const auto alpha = static_cast<float>(lua_tonumber(state, -1)) / 255.f;

    lua_pop(state, 5);

    if (alpha <= .0f) [[unlikely]]
      continue;

    const SDL_FColor color{1.f, 1.f, 1.f, alpha};
    const auto base = static_cast<int32_t>(vertices.size());

    vertices.emplace_back(SDL_Vertex{{x, y}, color, {.0f, .0f}});
    vertices.emplace_back(SDL_Vertex{{x + w, y}, color, {1.f, .0f}});
    vertices.emplace_back(SDL_Vertex{{x + w, y + h}, color, {1.f, 1.f}});
    vertices.emplace_back(SDL_Vertex{{x, y + h}, color, {.0f, 1.f}});

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
  auto *self = checkuserdata<foreground>(state, 1, "Foreground");
  const std::string_view key = luaL_checkstring(state, 2);

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
  lua_pushnumber(L, static_cast<lua_Number>(p.width()));
  lua_setfield(L, -2, "width");
  lua_pushnumber(L, static_cast<lua_Number>(p.height()));
  lua_setfield(L, -2, "height");
  lua_setfield(L, -2, "pixmap");

  _reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

  _on_appear = acquire(L, -1, "on_appear");
  _on_disappear = acquire(L, -1, "on_disappear");
  _on_loop = acquire(L, -1, "on_loop");
  _on_paint = acquire(L, -1, "on_paint");

  lua_pop(L, 1);

  pushuserdata(L, this, "Foreground");
  _userdata_reference = luaL_ref(L, LUA_REGISTRYINDEX);
}

foreground::~foreground() {
  disappear();
  release(L, _on_paint);
  release(L, _on_loop);
  release(L, _on_disappear);
  release(L, _on_appear);
  release(L, _reference);
  release(L, _userdata_reference);
}

void foreground::wire() {
  metatable(L, "Foreground", foreground_index);
}

void foreground::expose() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata_reference);
  lua_setglobal(L, "foreground");
}

void foreground::appear() {
  invoke(L, _on_appear, _reference);
  release(L, _on_appear);
}

void foreground::disappear() {
  invoke(L, _on_disappear, _reference);
  release(L, _on_disappear);
}

void foreground::update(float delta) {
  invoke(L, _on_loop, _reference, delta);
}

void foreground::draw() {
  invoke(L, _on_paint, _userdata_reference);
}
