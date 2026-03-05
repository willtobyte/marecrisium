#include "user.hpp"

static int friend_index(lua_State *state) {
  luaL_checkudata(state, 1, "Friend");
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "id") {
    lua_getfenv(state, 1);
    lua_getfield(state, -1, "id");
    return 1;
  }

  if (key == "name") {
    lua_getfenv(state, 1);
    lua_getfield(state, -1, "name");
    return 1;
  }

  return lua_pushnil(state), 1;
}

static int user_index(lua_State *state) {
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "persona") {
    const auto name = GetPersonaName();
    lua_pushstring(state, name.c_str());
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

      const auto name = GetFriendPersonaName(id);
      if (name.empty()) [[unlikely]]
        continue;

      lua_newuserdata(state, 1);
      luaL_getmetatable(state, "Friend");
      lua_setmetatable(state, -2);

      lua_newtable(state);
      lua_pushnumber(state, static_cast<lua_Number>(id));
      lua_setfield(state, -2, "id");
      lua_pushstring(state, name.c_str());
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
