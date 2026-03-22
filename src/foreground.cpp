#include "foreground.hpp"

namespace {
constexpr int STRIDE = 5;

int foreground_draw(lua_State *state) {
  auto **ptr = static_cast<foreground **>(luaL_checkudata(state, 1, "Foreground"));
  auto *self = *ptr;
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
  auto **ptr = static_cast<foreground **>(luaL_checkudata(state, 1, "Foreground"));
  auto *self = *ptr;
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "draw") {
    lua_pushcfunction(state, foreground_draw);
    return 1;
  }

  lua_rawgeti(state, LUA_REGISTRYINDEX, self->_reference);
  lua_getfield(state, -1, key.data());
  if (!lua_isnil(state, -1)) {
    lua_remove(state, -2);
    return 1;
  }
  lua_pop(state, 1);

  static std::array<char, 64> buffer;
  buffer[0] = 'o';
  buffer[1] = 'n';
  buffer[2] = '_';
  const auto length = key.size();
  std::memcpy(buffer.data() + 3, key.data(), length);
  buffer[3 + length] = '\0';

  lua_getfield(state, -1, buffer.data());
  lua_remove(state, -2);
  if (!lua_isnil(state, -1))
    return 1;
  lua_pop(state, 1);

  return lua_pushnil(state), 1;
}
}

foreground::foreground(std::string_view name) {
  _vertices.reserve(2048);
  _indices.reserve(3072);

  auto &p = depot->pixmap.get(std::format("foregrounds/{}/pixmap", name));
  _texture = &p;

  const auto filename = std::format("foregrounds/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
    std::string error{lua_tostring(L, -1)};
    lua_pop(L, 1);
    throw std::runtime_error{std::move(error)};
  }

  if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
    std::string error{lua_tostring(L, -1)};
    lua_pop(L, 1);
    throw std::runtime_error{std::move(error)};
  }

  lua_newtable(L);
  lua_pushnumber(L, static_cast<lua_Number>(p.width()));
  lua_setfield(L, -2, "width");
  lua_pushnumber(L, static_cast<lua_Number>(p.height()));
  lua_setfield(L, -2, "height");
  lua_setfield(L, -2, "pixmap");

  _reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

  lua_getfield(L, -1, "on_appear");
  if (lua_isfunction(L, -1))
    _on_appear = luaL_ref(L, LUA_REGISTRYINDEX);
  else
    lua_pop(L, 1);

  lua_getfield(L, -1, "on_disappear");
  if (lua_isfunction(L, -1))
    _on_disappear = luaL_ref(L, LUA_REGISTRYINDEX);
  else
    lua_pop(L, 1);

  lua_getfield(L, -1, "on_loop");
  if (lua_isfunction(L, -1))
    _on_loop = luaL_ref(L, LUA_REGISTRYINDEX);
  else
    lua_pop(L, 1);

  lua_getfield(L, -1, "on_paint");
  if (lua_isfunction(L, -1))
    _on_paint = luaL_ref(L, LUA_REGISTRYINDEX);
  else
    lua_pop(L, 1);

  lua_pop(L, 1);

  auto **ud = static_cast<foreground **>(lua_newuserdata(L, sizeof(foreground *)));
  *ud = this;

  luaL_getmetatable(L, "Foreground");
  lua_setmetatable(L, -2);
  _userdata_reference = luaL_ref(L, LUA_REGISTRYINDEX);
}

foreground::~foreground() {
  disappear();
  luaL_unref(L, LUA_REGISTRYINDEX, _on_paint);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_disappear);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_appear);
  luaL_unref(L, LUA_REGISTRYINDEX, _reference);
  luaL_unref(L, LUA_REGISTRYINDEX, _userdata_reference);
}

void foreground::wire() {
  luaL_newmetatable(L, "Foreground");
  lua_pushcfunction(L, foreground_index);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

void foreground::expose() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata_reference);
  lua_setglobal(L, "foreground");
}

void foreground::appear() {
  if (_on_appear == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, _on_appear);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

  luaL_unref(L, LUA_REGISTRYINDEX, _on_appear);
  _on_appear = LUA_NOREF;

  if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
    std::string error{lua_tostring(L, -1)};
    lua_pop(L, 1);
    throw std::runtime_error{std::move(error)};
  }
}

void foreground::disappear() {
  if (_on_disappear == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, _on_disappear);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

  luaL_unref(L, LUA_REGISTRYINDEX, _on_disappear);
  _on_disappear = LUA_NOREF;

  if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
    std::string error{lua_tostring(L, -1)};
    lua_pop(L, 1);
    throw std::runtime_error{std::move(error)};
  }
}

void foreground::update(float delta) {
  if (_on_loop != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    lua_pushnumber(L, static_cast<lua_Number>(delta));

    if (lua_pcall(L, 2, 0, 0) != 0) [[unlikely]] {
      std::string error{lua_tostring(L, -1)};
      lua_pop(L, 1);
      throw std::runtime_error{std::move(error)};
    }
  }
}

void foreground::draw() {
  if (_on_paint != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_paint);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata_reference);

    if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
      std::string error{lua_tostring(L, -1)};
      lua_pop(L, 1);
      throw std::runtime_error{std::move(error)};
    }
  }
}
