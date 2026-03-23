#include "platform.hpp"

static int platform_index(lua_State *state) {
  const auto key = std::string_view{luaL_checkstring(state, 2)};

  if (key == "name") {
    lua_pushstring(state, SDL_GetPlatform());
    return 1;
  }

  if (key == "cores") {
    lua_pushinteger(state, static_cast<lua_Integer>(SDL_GetNumLogicalCPUCores()));
    return 1;
  }

  if (key == "memory") {
    lua_pushinteger(state, static_cast<lua_Integer>(SDL_GetSystemRAM()));
    return 1;
  }

  if (key == "locale") {
    auto count = 0;
    const auto locales = std::unique_ptr<SDL_Locale*[], SDL_Deleter>{SDL_GetPreferredLocales(&count)};
    if (!locales || count == 0 || !locales[0]->language) [[unlikely]] {
      lua_pushstring(state, "");
      return 1;
    }

    if (locales[0]->country) {
      const auto result = std::format("{}-{}", locales[0]->language, locales[0]->country);
      lua_pushstring(state, result.c_str());
      return 1;
    }

    lua_pushstring(state, locales[0]->language);
    return 1;
  }

  if (key == "clipboard") {
    const auto text = std::unique_ptr<char, SDL_Deleter>{SDL_GetClipboardText()};
    lua_pushstring(state, text ? text.get() : "");
    return 1;
  }

  return lua_pushnil(state), 1;
}

static int platform_newindex(lua_State *state) {
  const auto key = std::string_view{luaL_checkstring(state, 2)};

  if (key == "clipboard") {
    const auto *text = luaL_checkstring(state, 3);
    SDL_SetClipboardText(text);
  }

  return 0;
}

void platform::wire() {
  metatable(L, "Platform", platform_index, platform_newindex);

  singleton(L, "Platform", "platform");
}
