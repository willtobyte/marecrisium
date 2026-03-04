#include "cassette.hpp"

namespace {
constexpr std::string_view TYPE_BOOL   = "bool";
constexpr std::string_view TYPE_NUMBER = "number";
constexpr std::string_view TYPE_STRING = "string";

constexpr const char* FILENAME = "cassette.tape";

using value_type = std::variant<bool, double, std::string>;

static std::unordered_map<std::string, value_type, transparent_hash, std::equal_to<>> data;

void encode_string_to(std::string_view str, std::string& out) {
  out.reserve(out.size() + str.size());
  for (const char c : str) {
    switch (c) {
      case '\n': {
        out.append("\\n");
        break;
      }
      case '\r': {
        out.append("\\r");
        break;
      }
      case '\\': {
        out.append("\\\\");
        break;
      }
      case '\'': {
        out.append("\\'");
        break;
      }
      default: {
        out.push_back(c);
        break;
      }
    }
  }
}

std::string decode_string(std::string_view str) {
  if (str.find('\\') == std::string_view::npos) [[likely]]
    return std::string(str);

  std::string result;
  result.reserve(str.size());
  for (auto i = 0uz; i < str.size(); ++i) {
    if (str[i] == '\\' && i + 1 < str.size()) {
      switch (str[i + 1]) {
        case 'n': {
          result += '\n';
          ++i;
          break;
        }
        case 'r': {
          result += '\r';
          ++i;
          break;
        }
        case '\\': {
          result += '\\';
          ++i;
          break;
        }
        case '\'': {
          result += '\'';
          ++i;
          break;
        }
        default: {
          result += str[i];
          break;
        }
      }
    } else {
      result += str[i];
    }
  }

  return result;
}

bool parse(std::string_view line, std::string_view& type, std::string_view& key, std::string_view& value) {
  if (line.empty())
    return false;

  const auto p = line.find(':');
  if (p == std::string_view::npos)
    return false;

  type = line.substr(0, p);

  const auto e = line.find('=', p + 1);
  if (e == std::string_view::npos)
    return false;

  key = line.substr(p + 1, e - p - 1);
  value = line.substr(e + 1);
  return !key.empty();
}

void load() {
  if (!std::filesystem::exists(FILENAME))
    return;

  std::ifstream file(FILENAME, std::ios::binary | std::ios::ate);
  if (!file)
    return;

  const auto size = file.tellg();
  file.seekg(0);
  std::string content(static_cast<std::size_t>(size), '\0');
  file.read(content.data(), size);

  std::string_view remaining{content};
  while (!remaining.empty()) {
    const auto pos = remaining.find('\n');
    const auto line = remaining.substr(0, pos);
    remaining = (pos == std::string_view::npos) ? std::string_view{} : remaining.substr(pos + 1);

    if (line.empty())
      continue;

    std::string_view type, key, value;
    if (!parse(line, type, key, value))
      continue;

    std::string str(key);

    if (type == TYPE_BOOL) {
      data.try_emplace(str, value == "1");
    } else if (type == TYPE_NUMBER) {
      char* end{};
      const auto v = std::strtod(value.data(), &end);
      data.try_emplace(str, v);
    } else if (type == TYPE_STRING) {
      data.try_emplace(str, decode_string(value));
    }
  }
}

void persist() {
  std::string buffer;
  buffer.reserve(data.size() * 64);

  for (const auto& [key, value] : data) {
    std::visit([&key, &buffer](const auto& v) {
      using T = std::decay_t<decltype(v)>;

      if constexpr (std::is_same_v<T, bool>) {
        std::format_to(std::back_inserter(buffer), "{}:{}={}\n", TYPE_BOOL, key, v ? '1' : '0');
      } else if constexpr (std::is_same_v<T, double>) {
        std::format_to(std::back_inserter(buffer), "{}:{}={}\n", TYPE_NUMBER, key, v);
      } else if constexpr (std::is_same_v<T, std::string>) {
        std::format_to(std::back_inserter(buffer), "{}:{}=", TYPE_STRING, key);
        encode_string_to(v, buffer);
        buffer.push_back('\n');
      }
    }, value);
  }

  std::ofstream file(FILENAME, std::ios::binary | std::ios::trunc);
  file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
}
}

static int cassette_clear(lua_State *) {
  data.clear();
  persist();
  return 0;
}

static int cassette_index(lua_State *state) {
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "clear")
    return lua_pushcfunction(state, cassette_clear), 1;

  const auto it = data.find(key);
  if (it == data.end()) [[unlikely]]
    return lua_pushnil(state), 1;

  return std::visit([state](const auto& v) -> int {
    using T = std::decay_t<decltype(v)>;

    if constexpr (std::is_same_v<T, bool>) {
      lua_pushboolean(state, v);
    } else if constexpr (std::is_same_v<T, double>) {
      lua_pushnumber(state, v);
    } else if constexpr (std::is_same_v<T, std::string>) {
      lua_pushstring(state, v.c_str());
    }

    return 1;
  }, it->second);
}

static int cassette_newindex(lua_State *state) {
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "clear") [[unlikely]]
    return 0;

  if (lua_isnil(state, 3)) {
    data.erase(std::string(key));
    persist();
    return 0;
  }

  if (lua_isboolean(state, 3)) {
    data.insert_or_assign(std::string(key), value_type{lua_toboolean(state, 3) != 0});
  } else if (lua_isnumber(state, 3)) {
    data.insert_or_assign(std::string(key), value_type{lua_tonumber(state, 3)});
  } else if (lua_isstring(state, 3)) {
    data.insert_or_assign(std::string(key), value_type{std::string(lua_tostring(state, 3))});
  }

  persist();
  return 0;
}

void cassette::wire() {
  load();

  lua_newuserdata(L, 1);

  luaL_newmetatable(L, "Cassette");
  lua_pushcfunction(L, cassette_index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, cassette_newindex);
  lua_setfield(L, -2, "__newindex");

  lua_setmetatable(L, -2);
  lua_setglobal(L, "cassette");
}
