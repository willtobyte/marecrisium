#include "binding.hpp"

constexpr int MAX_DEPTH = 6;
constexpr int MAX_ENTRIES = 10;

static void pretty(lua_State *state, luaL_Buffer *buffer, int index, int depth, std::vector<const void *> &visited) {
  const auto type = lua_type(state, index);

  switch (type) {
  case LUA_TSTRING:
    luaL_addstring(buffer, "\"");
    luaL_addstring(buffer, lua_tostring(state, index));
    luaL_addstring(buffer, "\"");
    break;

  case LUA_TNUMBER: {
    lua_pushvalue(state, index);
    luaL_addvalue(buffer);
  } break;

  case LUA_TBOOLEAN:
    luaL_addstring(buffer, lua_toboolean(state, index) ? "true" : "false");
    break;

  case LUA_TNIL:
    luaL_addstring(buffer, "nil");
    break;

  case LUA_TTABLE: {
    const auto *ptr = lua_topointer(state, index);
    if (std::find(visited.begin(), visited.end(), ptr) != visited.end()) {
      luaL_addstring(buffer, "(circular)");
      break;
    }

    if (depth >= MAX_DEPTH) {
      luaL_addstring(buffer, "{...}");
      break;
    }

    visited.push_back(ptr);

    luaL_addstring(buffer, "{ ");
    const auto abs = index > 0 ? index : lua_gettop(state) + index + 1;
    lua_pushnil(state);
    auto count = 0;
    while (lua_next(state, abs) != 0) {
      if (count >= MAX_ENTRIES) {
        luaL_addstring(buffer, "... ");
        lua_pop(state, 2);
        break;
      }

      if (count > 0)
        luaL_addstring(buffer, ", ");

      if (lua_type(state, -2) == LUA_TSTRING) {
        luaL_addstring(buffer, lua_tostring(state, -2));
        luaL_addstring(buffer, " = ");
      } else if (lua_type(state, -2) == LUA_TNUMBER) {
        luaL_addstring(buffer, "[");
        lua_pushvalue(state, -2);
        luaL_addvalue(buffer);
        luaL_addstring(buffer, "] = ");
      }

      pretty(state, buffer, lua_gettop(state), depth + 1, visited);
      lua_pop(state, 1);
      ++count;
    }

    luaL_addstring(buffer, " }");
    visited.pop_back();
  } break;

  case LUA_TUSERDATA: {
    if (lua_getmetatable(state, index)) {
      lua_getfield(state, -1, "__name");
      if (lua_isstring(state, -1)) {
        luaL_addstring(buffer, "(");
        luaL_addstring(buffer, lua_tostring(state, -1));
        luaL_addstring(buffer, ")");
      } else {
        luaL_addstring(buffer, "(userdata)");
      }
      lua_pop(state, 2);
    } else {
      luaL_addstring(buffer, "(userdata)");
    }
  } break;

  case LUA_TLIGHTUSERDATA: {
    std::array<char, 32> addr;
    std::snprintf(addr.data(), addr.size(), "(lightuserdata: %p)", lua_topointer(state, index));
    luaL_addstring(buffer, addr.data());
  } break;

  default:
    luaL_addstring(buffer, "(");
    luaL_addstring(buffer, lua_typename(state, type));
    luaL_addstring(buffer, ")");
    break;
  }
}

static int traceback(lua_State *state) {
  luaL_traceback(state, state, lua_tostring(state, 1), 1);

  luaL_Buffer buffer;
  luaL_buffinit(state, &buffer);
  luaL_addvalue(&buffer);

  std::vector<const void *> visited;
  visited.reserve(MAX_DEPTH);

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

      pretty(state, &buffer, -1, 0, visited);
      lua_pop(state, 1);
    }
  }

  luaL_pushresult(&buffer);
  return 1;
}

static int _traceback_ref = LUA_NOREF;

void binding::wire() {
  lua_pushcfunction(L, traceback);
  _traceback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

void pcall(lua_State *state, int nargs, int nresults) {
  const auto handler = lua_gettop(state) - nargs;
  lua_rawgeti(state, LUA_REGISTRYINDEX, _traceback_ref);
  lua_insert(state, handler);

  if (lua_pcall(state, nargs, nresults, handler) != 0) [[unlikely]] {
    std::string error{lua_tostring(state, -1)};
    lua_pop(state, 1);
    lua_remove(state, handler);
    throw std::runtime_error{std::move(error)};
  }

  lua_remove(state, handler);
}

void compile(lua_State *state, const std::vector<uint8_t> &buffer, std::string_view chunk) {
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();

  if (luaL_loadbuffer(state, data, size, chunk.data()) != 0) [[unlikely]] {
    std::string error{lua_tostring(state, -1)};
    lua_pop(state, 1);
    throw std::runtime_error{std::move(error)};
  }
}

void singleton(lua_State *state, const char *metatable, const char *global) noexcept {
  lua_newuserdata(state, 1);
  luaL_getmetatable(state, metatable);
  lua_setmetatable(state, -2);
  lua_setglobal(state, global);
}

void metatable(lua_State *state, const char *name, lua_CFunction index, lua_CFunction newindex, lua_CFunction gc) noexcept {
  luaL_newmetatable(state, name);

  if (index) {
    lua_pushcfunction(state, index);
    lua_setfield(state, -2, "__index");
  }

  if (newindex) {
    lua_pushcfunction(state, newindex);
    lua_setfield(state, -2, "__newindex");
  }

  if (gc) {
    lua_pushcfunction(state, gc);
    lua_setfield(state, -2, "__gc");
  }

  lua_pop(state, 1);
}
