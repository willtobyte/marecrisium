#include "platform.hpp"

static int platform_index(lua_State *state) {
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "name")
    return lua_pushstring(state, SDL_GetPlatform()), 1;

  if (key == "cores")
    return lua_pushinteger(state, SDL_GetNumLogicalCPUCores()), 1;

  if (key == "memory")
    return lua_pushinteger(state, SDL_GetSystemRAM()), 1;

  if (key == "locale") {
    auto count = 0;
    const auto locales = std::unique_ptr<SDL_Locale*[], SDL_Deleter>{SDL_GetPreferredLocales(&count)};
    if (!locales || count == 0 || !locales[0]->language) [[unlikely]]
      return lua_pushstring(state, ""), 1;

    if (locales[0]->country) {
      const auto result = std::format("{}-{}", locales[0]->language, locales[0]->country);
      lua_pushstring(state, result.c_str());
    } else {
      lua_pushstring(state, locales[0]->language);
    }

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
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "clipboard") {
    const auto *text = luaL_checkstring(state, 3);
    SDL_SetClipboardText(text);
  }

  return 0;
}

void platform::wire() {
  metatable(L, "Platform", platform_index, platform_newindex);

  lua_newuserdata(L, 1);
  luaL_getmetatable(L, "Platform");
  lua_setmetatable(L, -2);
  lua_setglobal(L, "platform");
}
