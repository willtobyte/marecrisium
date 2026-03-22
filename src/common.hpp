#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <numbers>
#include <limits>
#include <optional>
#include <print>
#include <queue>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <box2d/box2d.h>
#include <entt/entt.hpp>
#include <libwebsockets.h>
#include <lua.hpp>
#include <lauxlib.h>
#include <miniaudio.h>
#include <opusfile.h>
#include <physfs.h>
#include <SDL3/SDL.h>
#include <sentry.h>
#include <sqlite3.h>
#include <spng.h>
#include <yyjson.h>

#ifdef _MSC_VER
#  define noalias __restrict
#else
#  define noalias __restrict__
#endif

extern lua_State* L;
extern SDL_Renderer* renderer;
extern ma_engine* audioengine;

struct resources;
extern struct resources *depot;

struct viewport {
  float width;
  float height;
  float scale;
  float x;
  float y;
};

extern struct viewport viewport;

struct SDL_Deleter final {
  template <typename T>
  void operator()(T* ptr) const noexcept {
    if (!ptr) [[unlikely]] return;

    if constexpr (requires { SDL_CloseGamepad(ptr); }) SDL_CloseGamepad(ptr);
    else if constexpr (requires { SDL_DestroyTexture(ptr); }) SDL_DestroyTexture(ptr);
    else if constexpr (requires { SDL_free(ptr); }) SDL_free(ptr);
  }
};

struct SPNG_Deleter final {
  void operator()(spng_ctx* ctx) const noexcept {
    if (!ctx) [[unlikely]] return;

    spng_ctx_free(ctx);
  }
};

struct OggOpusFile_Deleter final {
  void operator()(OggOpusFile* ptr) const noexcept {
    if (!ptr) [[unlikely]] return;

    op_free(ptr);
  }
};

struct PHYSFS_Deleter final {
  template <typename T>
  void operator()(T* ptr) const noexcept {
    if (!ptr) [[unlikely]] return;

    if constexpr (std::is_same_v<T, PHYSFS_File>) {
      PHYSFS_close(ptr);
    } else if constexpr (std::is_same_v<T, char*>) {
      PHYSFS_freeList(ptr);
    }
  }
};

struct YYJSON_Deleter final {
  template <typename T>
  void operator()(T* ptr) const noexcept {
    if (!ptr) [[unlikely]] return;

    if constexpr (std::is_same_v<T, yyjson_doc>) yyjson_doc_free(ptr);
    else if constexpr (std::is_same_v<T, yyjson_mut_doc>) yyjson_mut_doc_free(ptr);
    else free(ptr);
  }
};

[[nodiscard]] float to_radians(float degrees) noexcept;

struct transparent_hash final {
  using is_transparent = void;
  auto operator()(std::string_view sv) const noexcept { return std::hash<std::string_view>{}(sv); }
};

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
  if constexpr (std::is_same_v<T, int>)
    lua_pushinteger(state, static_cast<lua_Integer>(first));
  else if constexpr (std::is_same_v<T, float>)
    lua_pushnumber(state, static_cast<lua_Number>(first));
  else if constexpr (std::is_same_v<T, bool>)
    lua_pushboolean(state, first ? 1 : 0);
  else if constexpr (std::is_same_v<T, const char *>)
    lua_pushstring(state, first);
  else if constexpr (std::is_same_v<T, std::string_view>)
    lua_pushlstring(state, first.data(), first.size());
  else if constexpr (std::is_same_v<T, std::string>)
    lua_pushlstring(state, first.data(), first.size());
  else
    static_assert(sizeof(T) == 0, "unsupported type for invoke");
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
