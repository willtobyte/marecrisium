#include "user.hpp"

namespace {
  namespace property {
    using entt::operator""_hs;

    constexpr auto id = "id"_hs;
    constexpr auto name = "name"_hs;
    constexpr auto persona = "persona"_hs;
    constexpr auto friends = "friends"_hs;
  }
}

static int friend_newindex(lua_State *state) {
  const auto *key = luaL_checkstring(state, 2);
  return luaL_error(state, "attempt to write read-only field '%s'", key);
}

static int user_newindex(lua_State *state) {
  const auto *key = luaL_checkstring(state, 2);
  return luaL_error(state, "attempt to write read-only field '%s'", key);
}

static int friend_index(lua_State *state) {
  luaL_checkudata(state, 1, "Friend");
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  switch (id) {
    case property::id:
      lua_getfenv(state, 1);
      lua_getfield(state, -1, "id");
      lua_remove(state, -2);
      return 1;

    case property::name:
      lua_getfenv(state, 1);
      lua_getfield(state, -1, "name");
      lua_remove(state, -2);
      return 1;

    default:
      return lua_pushnil(state), 1;
  }
}

static int user_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  switch (id) {
    case property::persona:
      lua_pushstring(state, GetPersonaName());
      return 1;

    case property::friends: {
      const auto count = GetFriendCount();

      lua_newtable(state);

      auto index = 1;
      for (auto i = 0; i < count; ++i) {
        const auto fid = GetFriendByIndex(i);
        if (fid == 0) [[unlikely]]
          continue;

        const auto name = std::string_view{GetFriendPersonaName(fid)};
        if (name.empty()) [[unlikely]]
          continue;

        lua_newuserdata(state, 1);
        luaL_getmetatable(state, "Friend");
        lua_setmetatable(state, -2);

        lua_newtable(state);
        lua_pushinteger(state, static_cast<lua_Integer>(fid));
        lua_setfield(state, -2, "id");
        lua_pushlstring(state, name.data(), name.size());
        lua_setfield(state, -2, "name");
        lua_setfenv(state, -2);

        lua_rawseti(state, -2, index++);
      }

      return 1;
    }

    default:
      return lua_pushnil(state), 1;
  }
}

void user::wire() {
  metatable(L, "Friend", friend_index, friend_newindex);

  metatable(L, "User", user_index, user_newindex);

  singleton(L, "User", "user");
}
