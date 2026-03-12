#include "locales.hpp"

static int translate(lua_State *state) {
  lua_pushvalue(state, lua_upvalueindex(1));
  lua_pushvalue(state, 1);
  lua_rawget(state, -2);

  if (lua_isnil(state, -1)) [[unlikely]] {
    lua_pop(state, 2);
    lua_pushvalue(state, 1);
  } else {
    lua_remove(state, -2);
  }

  return 1;
}

void locales::wire() {
  lua_newtable(L);

  auto count = 0;
  const auto preferred = std::unique_ptr<SDL_Locale*[], SDL_Deleter>(SDL_GetPreferredLocales(&count));

  if (preferred && count > 0) [[likely]] {
    const auto filename = std::format("locales/{}.lua", preferred[0]->language);

    if (io::exists(filename)) {
      const auto buffer = io::read(filename);
      const auto *data = reinterpret_cast<const char *>(buffer.data());
      const auto size = buffer.size();
      const auto label = std::format("@{}", filename);

      if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
        auto error = std::string{lua_tostring(L, -1)};
        lua_pop(L, 1);
        throw std::runtime_error(std::move(error));
      }

      if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
        auto error = std::string{lua_tostring(L, -1)};
        lua_pop(L, 1);
        throw std::runtime_error(std::move(error));
      }

      lua_remove(L, -2);
    }
  }

  lua_pushcclosure(L, translate, 1);
  lua_setglobal(L, "_");
}
