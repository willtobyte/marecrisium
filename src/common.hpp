#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
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
#include <utility>
#include <variant>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <box2d/box2d.h>
#include <enet/enet.h>
#include <entt/entt.hpp>

using namespace entt::literals;
#include <flatbuffers/flatbuffers.h>
#include <rpc_generated.h>
#include <lua.hpp>
#include <miniaudio.h>
#include <opusfile.h>
#include <physfs.h>
#include <SDL3/SDL.h>
#include <sentry.h>
#include <spng.h>
#include <sqlite3.h>
#include <yyjson.h>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

#include "compat.hpp"

extern lua_State* L;
extern SDL_Renderer* renderer;
extern ma_engine audio;

struct resources;
extern struct resources *depot;

struct viewport {
  float width;
  float height;
  float scale;
  float x;
  float y;

  constexpr bool operator==(const viewport&) const = default;
};

extern struct viewport viewport;

struct SDL_Deleter final {
  template <typename T>
    requires requires(T* p) { SDL_CloseGamepad(p); } ||
             requires(T* p) { SDL_DestroyTexture(p); } ||
             requires(T* p) { SDL_free(p); }
  void operator()(T* ptr) const {
    if (!ptr) [[unlikely]] return;

    if constexpr (requires { SDL_CloseGamepad(ptr); }) SDL_CloseGamepad(ptr);
    else if constexpr (requires { SDL_DestroyTexture(ptr); }) SDL_DestroyTexture(ptr);
    else if constexpr (requires { SDL_free(ptr); }) SDL_free(ptr);
  }
};

struct SPNG_Deleter final {
  void operator()(spng_ctx* context) const {
    if (!context) [[unlikely]] return;

    spng_ctx_free(context);
  }
};

struct OggOpusFile_Deleter final {
  void operator()(OggOpusFile* ptr) const {
    if (!ptr) [[unlikely]] return;

    op_free(ptr);
  }
};

struct PHYSFS_Deleter final {
  template <typename T>
  void operator()(T* ptr) const {
    if (!ptr) [[unlikely]] return;

    if constexpr (std::is_same_v<T, PHYSFS_File>) {
      PHYSFS_close(ptr);
    } else if constexpr (std::is_same_v<T, char*>) {
      PHYSFS_freeList(ptr);
    }
  }
};

template <typename T>
[[nodiscard("Angle conversion has no side effects")]]
constexpr T to_radians(T degrees) {
  if consteval {
    return degrees * (std::numbers::pi_v<T> / T{180});
  } else {
    static constinit auto factor = std::numbers::pi_v<T> / T{180};
    return degrees * factor;
  }
}
