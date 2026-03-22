#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <lua.hpp>
#include <lauxlib.h>

void pcall(lua_State *state, int nargs, int nresults);
void compile(lua_State *state, const std::vector<uint8_t> &buffer, std::string_view label);

template <typename T>
[[nodiscard]] T get(lua_State *state, int index, const char *name, T fallback = T{}) noexcept {
  static_assert(sizeof(T) == 0, "unsupported type for get<T>");
  return fallback;
}

template <>
[[nodiscard]] float get<float>(lua_State *state, int index, const char *name, float fallback) noexcept;

template <>
[[nodiscard]] bool get<bool>(lua_State *state, int index, const char *name, bool fallback) noexcept;

template <>
[[nodiscard]] int get<int>(lua_State *state, int index, const char *name, int fallback) noexcept;

template <>
[[nodiscard]] std::string_view get<std::string_view>(lua_State *state, int index, const char *name, std::string_view fallback) noexcept;

template <typename T>
[[nodiscard]] T get(lua_State *state, int index, int i) noexcept {
  static_assert(sizeof(T) == 0, "unsupported type for get<T>");
  return T{};
}

template <>
[[nodiscard]] float get<float>(lua_State *state, int index, int i) noexcept;

[[nodiscard]] int acquire(lua_State *state, int index, const char *name) noexcept;
void release(lua_State *state, int &handle) noexcept;
void bind(lua_State *state, const char *name, lua_CFunction fn, void *upvalue) noexcept;
void metatable(lua_State *state, const char *name, lua_CFunction index, lua_CFunction newindex = nullptr, lua_CFunction gc = nullptr) noexcept;

static void push(lua_State *) {}

template <typename T, typename... Rest>
static void push(lua_State *state, T first, Rest... rest) {
  if constexpr (std::is_same_v<T, bool>)
    lua_pushboolean(state, first ? 1 : 0);
  else if constexpr (std::is_integral_v<T>)
    lua_pushinteger(state, static_cast<lua_Integer>(first));
  else if constexpr (std::is_floating_point_v<T>)
    lua_pushnumber(state, static_cast<lua_Number>(first));
  else if constexpr (std::is_same_v<T, const char *>)
    lua_pushstring(state, first);
  else if constexpr (std::is_same_v<T, std::string_view>)
    lua_pushlstring(state, first.data(), first.size());
  else if constexpr (std::is_same_v<T, std::string>)
    lua_pushlstring(state, first.data(), first.size());
  else
    static_assert(sizeof(T) == 0, "unsupported type for push");

  push(state, rest...);
}

template <typename... Args>
void invoke(lua_State *state, int callback, int self, Args... args) {
  if (callback == LUA_NOREF) [[unlikely]]
    return;
  lua_rawgeti(state, LUA_REGISTRYINDEX, callback);
  lua_rawgeti(state, LUA_REGISTRYINDEX, self);
  push(state, args...);
  pcall(state, 1 + static_cast<int>(sizeof...(args)), 0);
}
