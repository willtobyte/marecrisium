#include "xorshift.hpp"

xorshift128 rng{0};

namespace {

int math_random(lua_State *state) {
  const auto argc = lua_gettop(state);

  switch (argc) {
    case 0:
      lua_pushnumber(state, static_cast<lua_Number>(rng({.0f, 1.f})));
      return 1;

    case 1: {
      const auto n = static_cast<int>(luaL_checkinteger(state, 1));
      if (n < 1) [[unlikely]]
        return luaL_error(state, "interval is empty");
      lua_pushinteger(state, static_cast<lua_Integer>(rng(1, n)));
      return 1;
    }

    default: {
      const auto minimum = static_cast<int>(luaL_checkinteger(state, 1));
      const auto maximum = static_cast<int>(luaL_checkinteger(state, 2));
      if (minimum > maximum) [[unlikely]]
        return luaL_error(state, "interval is empty");
      lua_pushinteger(state, static_cast<lua_Integer>(rng(minimum, maximum)));
      return 1;
    }
  }
}

int math_randomseed(lua_State *state) {
  const auto seed = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  rng.seed(seed);
  return 0;
}
}

xorshift128::xorshift128(uint32_t seed) noexcept
    : state{seed, seed ^ 0x6C078965u, seed ^ 0x714ACBFBu, seed ^ 0x48077044u} {}

void xorshift128::seed(uint32_t value) noexcept {
  state[0] = value;
  state[1] = value ^ 0x6C078965u;
  state[2] = value ^ 0x714ACBFBu;
  state[3] = value ^ 0x48077044u;
}

uint32_t xorshift128::operator()() noexcept {
  auto t = state[3];
  t ^= t << 11;
  t ^= t >> 8;
  state[3] = state[2];
  state[2] = state[1];
  state[1] = state[0];
  t ^= state[0];
  t ^= state[0] >> 19;
  state[0] = t;
  return t;
}

float xorshift128::operator()(std::pair<float, float> range) noexcept {
  constexpr auto scale = 1.f / 4294967296.f;
  const auto [minimum, maximum] = range;
  return minimum + (maximum - minimum) * (static_cast<float>((*this)()) * scale);
}

int xorshift128::operator()(int minimum, int maximum) noexcept {
  const auto range = static_cast<uint64_t>(maximum) - static_cast<uint64_t>(minimum) + 1;
  return minimum + static_cast<int>(static_cast<uint64_t>((*this)()) % range);
}

void xorshift128::wire() {
  lua_getglobal(L, "math");
  lua_pushcfunction(L, math_random);
  lua_setfield(L, -2, "random");
  lua_pushcfunction(L, math_randomseed);
  lua_setfield(L, -2, "randomseed");
  lua_pop(L, 1);
}
