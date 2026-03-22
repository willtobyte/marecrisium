#include "common.hpp"

lua_State *L = nullptr;
SDL_Renderer *renderer = nullptr;
ma_engine *audioengine = nullptr;
struct viewport viewport{};
struct resources *depot = nullptr;

float to_radians(float degrees) noexcept {
  return degrees * (std::numbers::pi_v<float> / 180.f);
}

void pcall(lua_State *state, int nargs, int nresults) {
  const auto handler = lua_gettop(state) - nargs;
  lua_pushcfunction(state, [](lua_State *state) -> int {
    luaL_traceback(state, state, lua_tostring(state, 1), 1);
    return 1;
  });
  lua_insert(state, handler);

  if (lua_pcall(state, nargs, nresults, handler) != 0) [[unlikely]] {
    std::string error{lua_tostring(state, -1)};
    lua_pop(state, 1);
    lua_remove(state, handler);
    throw std::runtime_error{std::move(error)};
  }

  lua_remove(state, handler);
}

void compile(lua_State *state, const std::vector<uint8_t> &buffer, std::string_view label) {
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();

  if (luaL_loadbuffer(state, data, size, label.data()) != 0) [[unlikely]] {
    std::string error{lua_tostring(state, -1)};
    lua_pop(state, 1);
    throw std::runtime_error{std::move(error)};
  }
}
