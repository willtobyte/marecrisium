#include "platform.hpp"

namespace {
  namespace property {
    using entt::operator""_hs;

    constexpr auto name      = "name"_hs.value();
    constexpr auto cores     = "cores"_hs.value();
    constexpr auto memory    = "memory"_hs.value();
    constexpr auto locale    = "locale"_hs.value();
    constexpr auto clipboard = "clipboard"_hs.value();
  }
}

static int platform_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)}.value();

  switch (id) {
    case property::name:
      lua_pushstring(state, SDL_GetPlatform());
      return 1;

    case property::cores:
      lua_pushinteger(state, static_cast<lua_Integer>(SDL_GetNumLogicalCPUCores()));
      return 1;

    case property::memory:
      lua_pushinteger(state, static_cast<lua_Integer>(SDL_GetSystemRAM()));
      return 1;

    case property::locale: {
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

    case property::clipboard: {
      const auto text = std::unique_ptr<char, SDL_Deleter>{SDL_GetClipboardText()};
      lua_pushstring(state, text ? text.get() : "");
      return 1;
    }

    default:
      return lua_pushnil(state), 1;
  }
}

static int platform_newindex(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)}.value();

  if (id == property::clipboard) {
    const auto *text = luaL_checkstring(state, 3);
    SDL_SetClipboardText(text);
  }

  return 0;
}

void platform::wire() {
  metatable(L, "Platform", platform_index, platform_newindex);

  singleton(L, "Platform", "platform");
}
