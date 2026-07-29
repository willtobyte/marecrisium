namespace {
constexpr auto depth = 6uz;
constexpr auto entries = 10;

struct trail final {
  std::array<const void *, depth> tables{};
  size_t size{};

  [[nodiscard]] bool contains(const void *table) const noexcept {
    for (size_t index = 0; index < size; ++index)
      if (tables[index] == table) [[unlikely]]
        return true;

    return false;
  }

  [[nodiscard]] bool push(const void *table) noexcept {
    if (size == tables.size()) [[unlikely]]
      return false;

    tables[size++] = table;
    return true;
  }

  void pop() noexcept { --size; }
};

void format(lua_State *state, std::string &text, int index, trail &path) {
  const auto type = lua_type(state, index);
  auto out = std::back_inserter(text);

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
    const auto *table = lua_topointer(state, index);
    if (path.contains(table)) [[unlikely]] {
      std::format_to(out, "(circular)");
      break;
    }

    if (!path.push(table)) [[unlikely]] {
      std::format_to(out, "{{...}}");
      break;
    }

    std::format_to(out, "{{ ");
    const auto position = index > 0 ? index : lua_gettop(state) + index + 1;
    lua_pushnil(state);
    auto count = 0;
    while (lua_next(state, position) != 0) {
      if (count >= entries) [[unlikely]] {
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

      format(state, text, -1, path);
      lua_pop(state, 1);
      ++count;
    }

    std::format_to(out, " }}");
    path.pop();
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

}

namespace traceback {
int build(lua_State* state) {
  luaL_traceback(state, state, lua_tostring(state, 1), 1);

  std::string trace = lua_tostring(state, -1);
  lua_pop(state, 1);
  trace.reserve(trace.size() + 256);

  trail path;
  auto out = std::back_inserter(trace);
  lua_Debug frame;
  for (int level = 1; lua_getstack(state, level, &frame); ++level) {
    lua_getinfo(state, "Sl", &frame);
    const auto *source = frame.short_src;

    auto locals = false;
    for (int index = 1;; ++index) {
      const auto *name = lua_getlocal(state, &frame, index);
      if (!name)
        break;

      if (name[0] == '(') {
        lua_pop(state, 1);
        continue;
      }

      if (!locals) [[unlikely]] {
        std::format_to(out, "\n    locals at {}:{}:", source, frame.currentline);
        locals = true;
      }

      std::format_to(out, "\n      {} = ", name);

      format(state, trace, -1, path);
      lua_pop(state, 1);
    }
  }

  lua_pushlstring(state, trace.data(), trace.size());
  return 1;
}

}
