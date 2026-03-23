#include "platform.hpp"

static int platform_index(lua_State *state) {
  const auto key = argument<std::string_view>(state, 2);

  if (key == "name")
    return push(state, SDL_GetPlatform());

  if (key == "cores")
    return push(state, SDL_GetNumLogicalCPUCores());

  if (key == "memory")
    return push(state, SDL_GetSystemRAM());

  if (key == "locale") {
    auto count = 0;
    const auto locales = std::unique_ptr<SDL_Locale*[], SDL_Deleter>{SDL_GetPreferredLocales(&count)};
    if (!locales || count == 0 || !locales[0]->language) [[unlikely]]
      return push(state, "");

    if (locales[0]->country) {
      const auto result = std::format("{}-{}", locales[0]->language, locales[0]->country);
      return push(state, result.c_str());
    }

    return push(state, locales[0]->language);
  }

  if (key == "clipboard") {
    const auto text = std::unique_ptr<char, SDL_Deleter>{SDL_GetClipboardText()};
    return push(state, text ? text.get() : "");
  }

  return lua_pushnil(state), 1;
}

static int platform_newindex(lua_State *state) {
  const auto key = argument<std::string_view>(state, 2);

  if (key == "clipboard") {
    const auto *text = argument<const char *>(state, 3);
    SDL_SetClipboardText(text);
  }

  return 0;
}

void platform::wire() {
  metatable(L, "Platform", platform_index, platform_newindex);

  singleton(L, "Platform", "platform");
}
