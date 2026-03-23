#include "achievement.hpp"

static int achievement_unlock(lua_State *state) {
  const auto *id = argument<const char *>(state, 2);

  if (!SteamUserStats()) [[unlikely]]
    return lua_pushboolean(state, false), 1;

  if (GetAchievement(id))
    return lua_pushboolean(state, true), 1;

  const auto result = SetAchievement(id);
  StoreStats();

  return lua_pushboolean(state, result), 1;
}

static int achievement_index(lua_State *state) {
  const auto key = argument<std::string_view>(state, 2);

  if (key == "unlock")
    return lua_pushcfunction(state, achievement_unlock), 1;

  return lua_pushnil(state), 1;
}

void achievement::wire() {
  metatable(L, "Achievement", achievement_index);

  singleton(L, "Achievement", "achievement");
}
