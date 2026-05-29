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
#include "deleter.hpp"

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
