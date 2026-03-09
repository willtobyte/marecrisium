#include "overlay.hpp"

static int overlay_label(lua_State *state) {
  auto **ptr = static_cast<overlay **>(luaL_checkudata(state, 1, "Overlay"));
  auto *self = *ptr;
  const auto* font = luaL_checkstring(state, 2);
  const auto* text = luaL_checkstring(state, 3);
  const auto x = static_cast<float>(luaL_checknumber(state, 4));
  const auto y = static_cast<float>(luaL_checknumber(state, 5));
  self->render_label(font, text, x, y);
  return 0;
}

static int overlay_index(lua_State *state) {
  luaL_checkudata(state, 1, "Overlay");
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "label") {
    lua_pushcfunction(state, overlay_label);
    return 1;
  }

  return lua_pushnil(state), 1;
}

overlay::overlay(std::string_view name)
    : _name(name) {
  const auto filename = std::format("overlays/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(std::move(error));
  }

  if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(std::move(error));
  }

  lua_getfield(L, -1, "fonts");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      if (lua_isstring(L, -1)) {
        std::ignore = resources.font.get(lua_tostring(L, -1));
      }

      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  _reference = luaL_ref(L, LUA_REGISTRYINDEX);
}

overlay::~overlay() {
  luaL_unref(L, LUA_REGISTRYINDEX, _reference);
}

void overlay::update(float delta) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_loop");

  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    lua_pushnumber(L, static_cast<lua_Number>(delta));

    if (lua_pcall(L, 2, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(std::move(error));
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void overlay::draw() const {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_paint");

  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

    if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(std::move(error));
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void overlay::wire() {
  auto **memory = static_cast<overlay **>(lua_newuserdata(L, sizeof(overlay *)));
  *memory = this;

  if (luaL_newmetatable(L, "Overlay")) {
    lua_pushcfunction(L, overlay_index);
    lua_setfield(L, -2, "__index");
  }

  lua_setmetatable(L, -2);
  lua_setglobal(L, "overlay");
}

void overlay::render_label(std::string_view family, std::string_view text, float x, float y) const {
  const auto &f = resources.font.get(family);
  f.draw(text, x, y);
}
