#pragma once

#include <algorithm>
#include <bit>
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <print>
#include <queue>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <variant>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <box2d/box2d.h>
#include <entt/entt.hpp>

using namespace entt::literals;
#include <libwebsockets.h>
#include <lua.hpp>
#include <miniaudio.h>
#include <opusfile.h>
#include <physfs.h>
#include <SDL3/SDL.h>
#include <sentry.h>
#include <spng.h>
#include <sqlite3.h>
#include <yyjson.h>
#include <zstd.h>

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

struct transparent_hash final {
  using is_transparent = void;
  using is_avalanching = void;
  auto operator()(std::string_view sv) const noexcept {
    return ankerl::unordered_dense::hash<std::string_view>{}(sv);
  }
};

template <typename T>
[[nodiscard]] constexpr T to_radians(T degrees) noexcept {
  return degrees * (std::numbers::pi_v<T> / T{180});
}
