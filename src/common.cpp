#include "common.hpp"

lua_State *L = nullptr;
SDL_Renderer *renderer = nullptr;
ma_engine *audioengine = nullptr;
struct viewport viewport{};
struct resources *depot = nullptr;

float to_radians(float degrees) noexcept {
  return degrees * (std::numbers::pi_v<float> / 180.f);
}

static int traceback(lua_State *state) {
  luaL_traceback(state, state, lua_tostring(state, 1), 1);

  luaL_Buffer buffer;
  luaL_buffinit(state, &buffer);
  luaL_addvalue(&buffer);

  lua_Debug debug;
  for (int level = 1; lua_getstack(state, level, &debug); ++level) {
    lua_getinfo(state, "Sl", &debug);

    bool locals = false;
    for (int i = 1;; ++i) {
      const auto *name = lua_getlocal(state, &debug, i);
      if (!name)
        break;

      if (name[0] == '(') {
        lua_pop(state, 1);
        continue;
      }

      if (!locals) {
        luaL_addstring(&buffer, "\n    locals at ");
        luaL_addstring(&buffer, debug.short_src);
        luaL_addstring(&buffer, ":");
        lua_pushinteger(state, debug.currentline);
        luaL_addvalue(&buffer);
        luaL_addstring(&buffer, ":");
        locals = true;
      }

      luaL_addstring(&buffer, "\n      ");
      luaL_addstring(&buffer, name);
      luaL_addstring(&buffer, " = ");

      const auto type = lua_type(state, -1);
      switch (type) {
      case LUA_TSTRING: {
        luaL_addstring(&buffer, "\"");
        luaL_addvalue(&buffer);
        luaL_addstring(&buffer, "\"");
      } break;

      case LUA_TNUMBER: {
        luaL_addvalue(&buffer);
      } break;

      case LUA_TBOOLEAN: {
        luaL_addstring(&buffer, lua_toboolean(state, -1) ? "true" : "false");
        lua_pop(state, 1);
      } break;

      case LUA_TNIL: {
        luaL_addstring(&buffer, "nil");
        lua_pop(state, 1);
      } break;

      default: {
        luaL_addstring(&buffer, "(");
        luaL_addstring(&buffer, lua_typename(state, type));
        luaL_addstring(&buffer, ")");
        lua_pop(state, 1);
      } break;
      }
    }
  }

  luaL_pushresult(&buffer);
  return 1;
}

void pcall(lua_State *state, int nargs, int nresults) {
  const auto handler = lua_gettop(state) - nargs;
  lua_pushcfunction(state, traceback);
  lua_insert(state, handler);

  if (lua_pcall(state, nargs, nresults, handler) != 0) [[unlikely]] {
    std::string error{lua_tostring(state, -1)};
    lua_pop(state, 1);
    lua_remove(state, handler);
    throw std::runtime_error{std::move(error)};
  }

  lua_remove(state, handler);
}

void compile(lua_State *state, const std::vector<uint8_t> &buffer, std::string_view label) {
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();

  if (luaL_loadbuffer(state, data, size, label.data()) != 0) [[unlikely]] {
    std::string error{lua_tostring(state, -1)};
    lua_pop(state, 1);
    throw std::runtime_error{std::move(error)};
  }
}
