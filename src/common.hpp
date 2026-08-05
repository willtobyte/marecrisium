#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <charconv>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <numbers>
#include <optional>
#include <print>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#ifdef __APPLE__
#include <dlfcn.h>
#endif
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define ENTT_ID_TYPE std::uint64_t
#include <entt/entt.hpp>
#include <lua.hpp>
#include <miniaudio.h>
#include <SDL3/SDL.h>
#include <simde/x86/sse2.h>
#include <sqlite3.h>
#include <stb_image.h>
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#include <yyjson.h>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

using namespace entt::literals;

extern lua_State* L;
extern SDL_Renderer* renderer;
extern ma_engine audio;

struct resources;
extern struct resources *depot;

#include "viewport.hpp"

extern struct viewport viewport;

#include "mcg64.hpp"

extern mcg64 prng;

#include "achievement.hpp"
#include "application.hpp"
#include "cassette.hpp"
#include "compat.hpp"
#include "components.hpp"
#include "deleter.hpp"
#include "font.hpp"
#include "fontpool.hpp"
#include "gamepad.hpp"
#include "io.hpp"
#include "keyboard.hpp"
#include "locales.hpp"
#include "marshal.hpp"
#include "mouse.hpp"
#include "object.hpp"
#include "overlay.hpp"
#include "particle.hpp"
#include "particlepool.hpp"
#include "particlesystem.hpp"
#include "pixmap.hpp"
#include "pixmappool.hpp"
#include "platform.hpp"
#include "scriptengine.hpp"
#include "sound.hpp"
#include "soundpool.hpp"
#include "spritesheet.hpp"
#include "spritesheetpool.hpp"
#include "steam.hpp"
#include "stringpool.hpp"
#include "runtime.hpp"
#include "timer.hpp"
#include "traceback.hpp"
#include "resources.hpp"
#include "scene.hpp"
#include "director.hpp"
#include "engine.hpp"
#include "error.hpp"
#include "user.hpp"
