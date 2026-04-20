#include "binding.hpp"

constexpr int MAX_DEPTH = 6;
constexpr int MAX_ENTRIES = 10;

static int _traceback = LUA_NOREF;

struct breadcrumbs final {
  std::array<const void *, MAX_DEPTH> _data{};
  int _size{0};

  bool contains(const void *ptr) const {
    for (int i = 0; i < _size; ++i)
      if (_data[static_cast<size_t>(i)] == ptr) [[unlikely]]
        return true;
    return false;
  }

  bool push(const void *ptr) {
    if (_size >= MAX_DEPTH) [[unlikely]]
      return false;

    _data[static_cast<size_t>(_size++)] = ptr;
    return true;
  }

  void pop() { --_size; }
};

static void pretty(lua_State *state, std::string &output, int index, int depth, breadcrumbs &visited) {
  const auto type = lua_type(state, index);
  auto out = std::back_inserter(output);

  switch (type) {
  case LUA_TSTRING:
    std::format_to(out, "\"{}\"", lua_tostring(state, index));
    break;

  case LUA_TNUMBER:
    std::format_to(out, "{:.14g}", lua_tonumber(state, index));
    break;

  case LUA_TBOOLEAN:
    std::format_to(out, "{}", lua_toboolean(state, index) ? "true" : "false");
    break;

  case LUA_TNIL:
    std::format_to(out, "nil");
    break;

  case LUA_TTABLE: {
    const auto *ptr = lua_topointer(state, index);
    if (visited.contains(ptr)) [[unlikely]] {
      std::format_to(out, "(circular)");
      break;
    }

    if (!visited.push(ptr)) [[unlikely]] {
      std::format_to(out, "{{...}}");
      break;
    }

    std::format_to(out, "{{ ");
    const auto abs = index > 0 ? index : lua_gettop(state) + index + 1;
    lua_pushnil(state);
    auto count = 0;
    while (lua_next(state, abs) != 0) {
      if (count >= MAX_ENTRIES) [[unlikely]] {
        std::format_to(out, "... ");
        lua_pop(state, 2);
        break;
      }

      if (count > 0) [[likely]]
        std::format_to(out, ", ");

      if (lua_type(state, -2) == LUA_TSTRING) [[likely]] {
        std::format_to(out, "{} = ", lua_tostring(state, -2));
      } else if (lua_type(state, -2) == LUA_TNUMBER) {
        std::format_to(out, "[{:.14g}] = ", lua_tonumber(state, -2));
      }

      pretty(state, output, lua_gettop(state), depth + 1, visited);
      lua_pop(state, 1);
      ++count;
    }

    std::format_to(out, " }}");
    visited.pop();
  } break;

  case LUA_TUSERDATA: {
    if (!lua_getmetatable(state, index)) {
      std::format_to(out, "(userdata)");
      break;
    }

    lua_getfield(state, -1, "__name");
    if (!lua_isstring(state, -1)) [[unlikely]] {
      std::format_to(out, "(userdata)");
      lua_pop(state, 2);
      break;
    }

    std::format_to(out, "({})", lua_tostring(state, -1));
    lua_pop(state, 2);
  } break;

  case LUA_TLIGHTUSERDATA: {
    std::format_to(out, "(lightuserdata: {})", lua_topointer(state, index));
  } break;

  default:
    std::format_to(out, "({})", lua_typename(state, type));
    break;
  }
}

static int traceback(lua_State *state) {
  luaL_traceback(state, state, lua_tostring(state, 1), 1);

  std::string result = lua_tostring(state, -1);
  lua_pop(state, 1);
  result.reserve(result.size() + 256);

  breadcrumbs visited;
  auto out = std::back_inserter(result);

  lua_Debug debug;
  for (int level = 1; lua_getstack(state, level, &debug); ++level) {
    lua_getinfo(state, "Sl", &debug);
    const auto *source = static_cast<const char *>(debug.short_src);

    bool has_locals = false;
    for (int i = 1;; ++i) {
      const auto *name = lua_getlocal(state, &debug, i);
      if (!name)
        break;

      if (name[0] == '(') {
        lua_pop(state, 1);
        continue;
      }

      if (!has_locals) [[unlikely]] {
        std::format_to(out, "\n    locals at {}:{}:", source, debug.currentline);
        has_locals = true;
      }

      std::format_to(out, "\n      {} = ", name);

      pretty(state, result, -1, 0, visited);
      lua_pop(state, 1);
    }
  }

  lua_pushlstring(state, result.data(), result.size());
  return 1;
}

[[noreturn]] static void raise(lua_State *state) {
  const char *message = lua_tostring(state, -1);
  std::string error{message ? message : "unknown lua error"};
  lua_pop(state, 1);
  throw std::runtime_error{std::move(error)};
}

void binding::wire() {
  lua_atpanic(L, +[](lua_State *state) -> int {
    raise(state);
  });

  lua_pushcfunction(L, traceback);
  _traceback = luaL_ref(L, LUA_REGISTRYINDEX);
}

bool pcall(lua_State *state, int nargs, int nresults, fault mode) {
  const auto handler = lua_gettop(state) - nargs;
  lua_rawgeti(state, LUA_REGISTRYINDEX, _traceback);
  lua_insert(state, handler);

  const auto result = lua_pcall(state, nargs, nresults, handler);
  lua_remove(state, handler);

  if (result == LUA_OK) [[likely]]
    return true;

  if (mode == fault::ignore) {
    lua_pop(state, 1);
    return false;
  }

  raise(state);
}

void compile(lua_State *state, std::span<const uint8_t> buffer, std::string_view chunk) {
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();

  if (luaL_loadbuffer(state, data, size, chunk.data())) [[unlikely]]
    raise(state);
}

void singleton(lua_State *state, const char *metatable, const char *global) {
  lua_newuserdata(state, 1);
  luaL_getmetatable(state, metatable);
  lua_setmetatable(state, -2);
  lua_setglobal(state, global);
}

void metatable(lua_State *state, const char *name, lua_CFunction index, lua_CFunction newindex, lua_CFunction gc) {
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
