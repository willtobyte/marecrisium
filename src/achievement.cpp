#include "achievement.hpp"

namespace {
  namespace property {
    using entt::operator""_hs;

    constexpr auto unlock = "unlock"_hs.value();
  }
}

static int achievement_unlock(lua_State *state) {
  const auto *id = luaL_checkstring(state, 2);

  if (!SteamUserStats()) [[unlikely]] {
    lua_pushboolean(state, 0);
    return 1;
  }

  if (GetAchievement(id)) {
    lua_pushboolean(state, 1);
    return 1;
  }

  const auto result = SetAchievement(id);
  StoreStats();

  lua_pushboolean(state, result ? 1 : 0);
  return 1;
}

static int achievement_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)}.value();

  if (id == property::unlock)
    return lua_pushcfunction(state, achievement_unlock), 1;

  return lua_pushnil(state), 1;
}

void achievement::wire() {
  metatable(L, "Achievement", achievement_index);

  singleton(L, "Achievement", "achievement");
}
