#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <lua.hpp>
#include <lauxlib.h>

void pcall(lua_State *state, int nargs, int nresults);
void compile(lua_State *state, const std::vector<uint8_t> &buffer, std::string_view label);
void singleton(lua_State *state, const char *metatable, const char *global) noexcept;
void callback(lua_State *state, int argument, int &reference);
int dispatch(lua_State *state, int reference, std::string_view key);
[[nodiscard]] std::pair<float, float> checkvec2(lua_State *state, int index);
void pushvec2(lua_State *state, float x, float y);

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

template <typename T>
void pushuserdata(lua_State *state, T *ptr, const char *name) noexcept {
  auto **memory = static_cast<T **>(lua_newuserdata(state, sizeof(T *)));
  *memory = ptr;
  luaL_getmetatable(state, name);
  lua_setmetatable(state, -2);
}

template <typename T>
auto check(lua_State *state, int index, const char *name = nullptr) {
  if constexpr (std::is_same_v<T, float>)
    return static_cast<float>(luaL_checknumber(state, index));
  else if constexpr (std::is_same_v<T, int>)
    return static_cast<int>(luaL_checkinteger(state, index));
  else if constexpr (std::is_same_v<T, bool>)
    return lua_toboolean(state, index) != 0;
  else if constexpr (std::is_same_v<T, const char *>)
    return luaL_checkstring(state, index);
  else if constexpr (std::is_same_v<T, std::string_view>)
    return std::string_view{luaL_checkstring(state, index)};
  else if constexpr (std::is_void_v<T>)
    return luaL_checkudata(state, index, name);
  else if constexpr (std::is_trivially_copyable_v<T>)
    return static_cast<T *>(luaL_checkudata(state, index, name));
  else
    return *static_cast<T **>(luaL_checkudata(state, index, name));
}

template <typename T>
[[nodiscard]] T *upvalue(lua_State *state) noexcept {
  return static_cast<T *>(lua_touserdata(state, lua_upvalueindex(1)));
}

template <typename T>
int push(lua_State *state, T value) noexcept {
  if constexpr (std::is_same_v<T, bool>)
    lua_pushboolean(state, value ? 1 : 0);
  else if constexpr (std::is_integral_v<T>)
    lua_pushinteger(state, static_cast<lua_Integer>(value));
  else if constexpr (std::is_floating_point_v<T>)
    lua_pushnumber(state, static_cast<lua_Number>(value));
  else if constexpr (std::is_same_v<T, const char *>)
    lua_pushstring(state, value);
  else if constexpr (std::is_convertible_v<T, std::string_view>) {
    const auto sv = std::string_view{value};
    lua_pushlstring(state, sv.data(), sv.size());
  } else
    static_assert(sizeof(T) == 0, "unsupported type for push");
  return 1;
}

template <typename... Args>
void invoke(lua_State *state, int callback, int self, Args... args) {
  if (callback == LUA_NOREF) [[unlikely]]
    return;
  lua_rawgeti(state, LUA_REGISTRYINDEX, callback);
  lua_rawgeti(state, LUA_REGISTRYINDEX, self);
  (push(state, args), ...);
  pcall(state, 1 + static_cast<int>(sizeof...(args)), 0);
}

template <typename... Args>
void fire(lua_State *state, int callback, Args... args) {
  if (callback == LUA_NOREF) [[unlikely]]
    return;
  lua_rawgeti(state, LUA_REGISTRYINDEX, callback);
  (push(state, args), ...);
  pcall(state, static_cast<int>(sizeof...(args)), 0);
}

[[nodiscard]] inline int capture(lua_State *state, int index) {
  luaL_checktype(state, index, LUA_TFUNCTION);
  lua_pushvalue(state, index);
  return luaL_ref(state, LUA_REGISTRYINDEX);
}

template <typename... Args>
void call(lua_State *state, int prototype, int handle, std::string_view method, Args... args) {
  if (prototype == LUA_NOREF || handle == LUA_NOREF) [[unlikely]]
    return;
  lua_rawgeti(state, LUA_REGISTRYINDEX, prototype);
  lua_getfield(state, -1, method.data());
  if (lua_isfunction(state, -1)) {
    lua_rawgeti(state, LUA_REGISTRYINDEX, handle);
    (push(state, args), ...);
    pcall(state, 1 + static_cast<int>(sizeof...(args)), 0);
  } else {
    lua_pop(state, 1);
  }
  lua_pop(state, 1);
}
