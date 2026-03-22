#include "foreground.hpp"

namespace {
struct sprite {
  pixmap *texture{nullptr};
  float x{};
  float y{};
  float width{};
  float height{};
  uint8_t alpha{255};
  double angle{};
};

static_assert(std::is_trivially_copyable_v<sprite>);

int sprite_index(lua_State *state) {
  auto *s = static_cast<sprite *>(luaL_checkudata(state, 1, "ForegroundSprite"));
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "x") {
    lua_pushnumber(state, static_cast<lua_Number>(s->x));
    return 1;
  }

  if (key == "y") {
    lua_pushnumber(state, static_cast<lua_Number>(s->y));
    return 1;
  }

  if (key == "width") {
    lua_pushnumber(state, static_cast<lua_Number>(s->width));
    return 1;
  }

  if (key == "height") {
    lua_pushnumber(state, static_cast<lua_Number>(s->height));
    return 1;
  }

  if (key == "alpha") {
    lua_pushnumber(state, static_cast<lua_Number>(s->alpha));
    return 1;
  }

  if (key == "angle") {
    lua_pushnumber(state, static_cast<lua_Number>(s->angle));
    return 1;
  }

  return lua_pushnil(state), 1;
}

int sprite_newindex(lua_State *state) {
  auto *s = static_cast<sprite *>(luaL_checkudata(state, 1, "ForegroundSprite"));
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "x") {
    s->x = static_cast<float>(luaL_checknumber(state, 3));
    return 0;
  }

  if (key == "y") {
    s->y = static_cast<float>(luaL_checknumber(state, 3));
    return 0;
  }

  if (key == "alpha") {
    s->alpha = static_cast<uint8_t>(luaL_checknumber(state, 3));
    return 0;
  }

  if (key == "angle") {
    s->angle = static_cast<double>(luaL_checknumber(state, 3));
    return 0;
  }

  return luaL_error(state, "ForegroundSprite: unknown property '%s'", key.data());
}

int foreground_draw(lua_State *state) {
  auto **ptr = static_cast<foreground **>(luaL_checkudata(state, 1, "Foreground"));
  auto *self = *ptr;
  auto *s = static_cast<sprite *>(luaL_checkudata(state, 2, "ForegroundSprite"));
  const auto x = static_cast<float>(luaL_checknumber(state, 3));
  const auto y = static_cast<float>(luaL_checknumber(state, 4));
  const auto width = static_cast<float>(luaL_checknumber(state, 5));
  const auto height = static_cast<float>(luaL_checknumber(state, 6));
  const auto alpha = lua_isnumber(state, 7)
    ? static_cast<uint8_t>(lua_tonumber(state, 7))
    : static_cast<uint8_t>(255);
  const auto angle = lua_isnumber(state, 8)
    ? static_cast<double>(lua_tonumber(state, 8))
    : .0;

  self->enqueue(s->texture, x, y, width, height, alpha, angle);
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
  _batch.reserve(256);

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

  lua_getfield(L, -1, "pixmaps");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      if (lua_isstring(L, -1)) {
        const std::string pixmap_name{lua_tostring(L, -1)};
        auto &pm = depot->pixmap.get(std::format("foregrounds/{}", pixmap_name));

        lua_pop(L, 1);

        auto *memory = static_cast<sprite *>(lua_newuserdata(L, sizeof(sprite)));
        new (memory) sprite{};
        memory->texture = &pm;
        memory->width = static_cast<float>(pm.width());
        memory->height = static_cast<float>(pm.height());

        luaL_getmetatable(L, "ForegroundSprite");
        lua_setmetatable(L, -2);

        lua_setfield(L, -3, pixmap_name.c_str());
      } else {
        lua_pop(L, 1);
      }
    }
  }
  lua_pop(L, 1);

  _reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

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

  auto **memory = static_cast<foreground **>(lua_newuserdata(L, sizeof(foreground *)));
  *memory = this;

  luaL_getmetatable(L, "Foreground");
  lua_setmetatable(L, -2);
  _userdata_reference = luaL_ref(L, LUA_REGISTRYINDEX);
}

foreground::~foreground() {
  luaL_unref(L, LUA_REGISTRYINDEX, _on_paint);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  luaL_unref(L, LUA_REGISTRYINDEX, _reference);
  luaL_unref(L, LUA_REGISTRYINDEX, _userdata_reference);
}

void foreground::wire() {
  luaL_newmetatable(L, "ForegroundSprite");
  lua_pushcfunction(L, sprite_index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, sprite_newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pop(L, 1);

  luaL_newmetatable(L, "Foreground");
  lua_pushcfunction(L, foreground_index);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

void foreground::expose() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata_reference);
  lua_setglobal(L, "foreground");
}

void foreground::enqueue(pixmap *texture, float x, float y, float width, float height, uint8_t alpha, double angle) {
  _batch.push_back({texture, x, y, width, height, alpha, angle});
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
  _batch.clear();

  if (_on_paint != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_paint);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata_reference);

    if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
      std::string error{lua_tostring(L, -1)};
      lua_pop(L, 1);
      throw std::runtime_error{std::move(error)};
    }
  }

  for (const auto &dc : _batch) {
    if (dc.alpha > 0 && dc.texture) {
      const auto pw = static_cast<float>(dc.texture->width());
      const auto ph = static_cast<float>(dc.texture->height());
      dc.texture->draw(
        .0f, .0f, pw, ph,
        dc.x, dc.y, dc.width, dc.height,
        dc.angle, dc.alpha
      );
    }
  }
}
