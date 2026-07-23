namespace {
constexpr auto max_depth = 6uz;
constexpr auto max_entries = 10;

int traceback_reference{};

struct breadcrumbs final {
  std::array<const void *, max_depth> data{};
  size_t size{};

  [[nodiscard]] bool contains(const void *ptr) const noexcept {
    for (size_t i = 0; i < size; ++i)
      if (data[i] == ptr) [[unlikely]]
        return true;

    return false;
  }

  [[nodiscard]] bool push(const void *ptr) noexcept {
    if (size == data.size()) [[unlikely]]
      return false;

    data[size++] = ptr;
    return true;
  }

  void pop() noexcept { --size; }
};

void pretty(lua_State *state, std::string &output, int index, breadcrumbs &visited) {
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
      if (count >= max_entries) [[unlikely]] {
        std::format_to(out, "... ");
        lua_pop(state, 2);
        break;
      }

      if (count > 0) [[likely]]
        std::format_to(out, ", ");

      const auto key = lua_type(state, -2);
      if (key == LUA_TSTRING) [[likely]] {
        std::format_to(out, "{} = ", lua_tostring(state, -2));
      } else if (key == LUA_TNUMBER) {
        std::format_to(out, "[{:.14g}] = ", lua_tonumber(state, -2));
      }

      pretty(state, output, -1, visited);
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

    lua_pushliteral(state, "__name");
    lua_rawget(state, -2);
    if (!lua_isstring(state, -1)) [[unlikely]] {
      std::format_to(out, "(userdata)");
      lua_pop(state, 2);
      break;
    }

    std::format_to(out, "({})", lua_tostring(state, -1));
    lua_pop(state, 2);
  } break;

  case LUA_TLIGHTUSERDATA:
    std::format_to(out, "(lightuserdata: {})", lua_topointer(state, index));
    break;

  default:
    std::format_to(out, "({})", lua_typename(state, type));
    break;
  }
}

int traceback(lua_State *state) {
  luaL_traceback(state, state, lua_tostring(state, 1), 1);

  std::string result = lua_tostring(state, -1);
  lua_pop(state, 1);
  result.reserve(result.size() + 256);

  breadcrumbs visited;
  auto out = std::back_inserter(result);
  lua_Debug debug;
  for (int level = 1; lua_getstack(state, level, &debug); ++level) {
    lua_getinfo(state, "Sl", &debug);
    const auto *source = debug.short_src;

    auto has_locals = false;
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

      pretty(state, result, -1, visited);
      lua_pop(state, 1);
    }
  }

  lua_pushlstring(state, result.data(), result.size());
  return 1;
}

[[noreturn]] void raise(lua_State *state) {
  const auto *message = lua_tostring(state, -1);
  auto error = std::runtime_error{message ? message : "unknown lua error"};
  lua_pop(state, 1);
  throw error;
}

#ifdef _MSC_VER
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void prepare(lua_State *state) {
  if (traceback_reference == 0) [[unlikely]] {
    lua_atpanic(state, [](lua_State *error) -> int {
      raise(error);
    });

    lua_pushcfunction(state, traceback);
    traceback_reference = luaL_ref(state, LUA_REGISTRYINDEX);
  }

}

int trampoline(lua_State *state) {
  const auto arguments = lua_gettop(state);
  lua_pushvalue(state, lua_upvalueindex(1));
  lua_insert(state, 1);

  try {
    lua_call(state, arguments, LUA_MULTRET);
  } catch (const std::exception &error) {
    return luaL_error(state, "%s", error.what());
  }

  return lua_gettop(state);
}

bool invoke(lua_State *state, int arguments, int results) {
  assert(traceback_reference != 0);
  const auto position = lua_gettop(state) - arguments;
  lua_rawgeti(state, LUA_REGISTRYINDEX, traceback_reference);
  lua_insert(state, position);

  const auto status = lua_pcall(state, arguments, results, position);
  lua_remove(state, position);

  if (status == LUA_OK) [[likely]]
    return true;

  raise(state);
}
}

namespace binding {
bool call(lua_State *state, int arguments, int results) {
  return invoke(state, arguments, results);
}

bool call(lua_State *state, int arguments, int results, fault mode) {
  if (mode == fault::fatal)
    return invoke(state, arguments, results);

  const auto status = lua_pcall(state, arguments, results, 0);
  if (status == LUA_OK) [[likely]]
    return true;

  lua_pop(state, 1);
  return false;
}

void load(lua_State *state, std::span<const uint8_t> buffer, const char *chunk) {
  prepare(state);
  const auto *data = reinterpret_cast<const char *>(buffer.data());

  if (luaL_loadbuffer(state, data, buffer.size(), chunk) != LUA_OK) [[unlikely]]
    raise(state);
}

void singleton(lua_State *state, const char *metatable, const char *global) {
  lua_newuserdata(state, 1);
  luaL_getmetatable(state, metatable);
  lua_setmetatable(state, -2);
  lua_setglobal(state, global);
}

void callback(lua_State *state, lua_CFunction native) {
  prepare(state);
  lua_pushcfunction(state, native);
  lua_pushcclosure(state, trampoline, 1);
}

void closure(lua_State *state, lua_CFunction native, int upvalues) {
  prepare(state);
  lua_pushcclosure(state, native, upvalues);
  lua_pushcclosure(state, trampoline, 1);
}

#ifdef _MSC_VER
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void metatable(lua_State *state, const char *name, lua_CFunction index, lua_CFunction newindex, lua_CFunction gc) {
  if (luaL_newmetatable(state, name)) {
    lua_pushstring(state, name);
    lua_setfield(state, -2, "__name");
  }

  if (index) {
    callback(state, index);
    lua_setfield(state, -2, "__index");
  }

  if (newindex) {
    callback(state, newindex);
    lua_setfield(state, -2, "__newindex");
  }

  if (gc) {
    callback(state, gc);
    lua_setfield(state, -2, "__gc");
  }

  lua_pop(state, 1);
}
}
