#pragma once

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numbers>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
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
#include <spng.h>
#include <yyjson.h>

extern lua_State* L;
extern SDL_Renderer* renderer;
extern ma_engine* audioengine;

struct viewport {
  float width;
  float height;
  float scale;
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
  auto operator()(std::string_view sv) const noexcept { return std::hash<std::string_view>{}(sv); }
};
