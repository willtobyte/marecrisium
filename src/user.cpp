#include "user.hpp"

static int friend_index(lua_State *state) {
  luaL_checkudata(state, 1, "Friend");
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "id") {
    lua_getfenv(state, 1);
    lua_getfield(state, -1, "id");
    lua_remove(state, -2);
    return 1;
  }

  if (key == "name") {
    lua_getfenv(state, 1);
    lua_getfield(state, -1, "name");
    lua_remove(state, -2);
    return 1;
  }

  return lua_pushnil(state), 1;
}

static int user_index(lua_State *state) {
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "persona") {
    lua_pushstring(state, GetPersonaName());
    return 1;
  }

  if (key == "friends") {
    const auto count = GetFriendCount();

    lua_newtable(state);
    auto index = 1;

    for (auto i = 0; i < count; ++i) {
      const auto id = GetFriendByIndex(i);
      if (id == 0) [[unlikely]]
        continue;

      const std::string_view name = GetFriendPersonaName(id);
      if (name.empty()) [[unlikely]]
        continue;

      lua_newuserdata(state, 1);
      luaL_getmetatable(state, "Friend");
      lua_setmetatable(state, -2);

      lua_newtable(state);
      lua_pushnumber(state, static_cast<lua_Number>(id));
      lua_setfield(state, -2, "id");
      lua_pushstring(state, name.data());
      lua_setfield(state, -2, "name");
      lua_setfenv(state, -2);

      lua_rawseti(state, -2, index++);
    }

    return 1;
  }

  return lua_pushnil(state), 1;
}

void user::wire() {
  luaL_newmetatable(L, "Friend");
  lua_pushcfunction(L, friend_index);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);

  lua_newuserdata(L, 1);

  luaL_newmetatable(L, "User");
  lua_pushcfunction(L, user_index);
  lua_setfield(L, -2, "__index");

  lua_setmetatable(L, -2);
  lua_setglobal(L, "user");
}
