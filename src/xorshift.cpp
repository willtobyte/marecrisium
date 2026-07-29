xorshift128 rng{};

namespace {
static int random_callback(lua_State *state) {
  const auto argc = lua_gettop(state);

  switch (argc) {
    case 0:
      lua_pushnumber(state, static_cast<lua_Number>(rng({.0f, 1.f})));
      return 1;

    case 1: {
      const auto maximum = static_cast<int>(std::clamp(
        luaL_checkinteger(state, 1),
        lua_Integer{1},
        static_cast<lua_Integer>(std::numeric_limits<int>::max())));
      lua_pushinteger(state, static_cast<lua_Integer>(rng(1, maximum)));
      return 1;
    }

    default: {
      constexpr auto lower = static_cast<lua_Integer>(std::numeric_limits<int>::min());
      constexpr auto upper = static_cast<lua_Integer>(std::numeric_limits<int>::max());
      const auto minimum = static_cast<int>(std::clamp(luaL_checkinteger(state, 1), lower, upper));
      const auto maximum = static_cast<int>(std::clamp(luaL_checkinteger(state, 2), lower, upper));
      assert(minimum <= maximum && "random range must be ordered");
      [[assume(minimum <= maximum)]];
      lua_pushinteger(state, static_cast<lua_Integer>(rng(minimum, maximum)));
      return 1;
    }
  }
}

static int randomseed_callback(lua_State *state) {
  const auto seed = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  rng.seed(seed);
  return 0;
}
}

void xorshift128::seed(uint32_t value) {
  constexpr auto increment = uint64_t{0x9E3779B97F4A7C15};
  constexpr auto first = uint64_t{0xBF58476D1CE4E5B9};
  constexpr auto second = uint64_t{0x94D049BB133111EB};

  auto seed = static_cast<uint64_t>(value);
  for (auto &word : state) {
    seed += increment;
    auto z = seed;
    z = (z ^ (z >> 30)) * first;
    z = (z ^ (z >> 27)) * second;
    z ^= z >> 31;
    word = static_cast<uint32_t>(z);
  }
}

[[nodiscard("RNG result should be used")]] uint32_t xorshift128::operator()() {
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

[[nodiscard("RNG result should be used")]] float xorshift128::operator()(std::pair<float, float> range) {
  constexpr auto scale = 0x1.fffffep-33f;
  const auto [minimum, maximum] = range;
  return minimum + (maximum - minimum) * (static_cast<float>((*this)()) * scale);
}

[[nodiscard("RNG result should be used")]] int xorshift128::operator()(int minimum, int maximum) {
  assert(minimum <= maximum && "random range must be ordered");
  [[assume(minimum <= maximum)]];

  const auto range = static_cast<uint32_t>(maximum) - static_cast<uint32_t>(minimum) + 1u;
  auto raw = (*this)();
  if (range == 0) [[unlikely]]
    return static_cast<int>(static_cast<int64_t>(minimum) + raw);

  auto product = static_cast<uint64_t>(raw) * static_cast<uint64_t>(range);
  auto low = static_cast<uint32_t>(product);
  if (low < range) [[unlikely]] {
    const auto threshold = (-range) % range;
    while (low < threshold) [[unlikely]] {
      raw = (*this)();
      product = static_cast<uint64_t>(raw) * static_cast<uint64_t>(range);
      low = static_cast<uint32_t>(product);
    }
  }

  return static_cast<int>(static_cast<int64_t>(minimum) + static_cast<int64_t>(product >> 32));
}

void xorshift128::wire() {
  lua_getglobal(L, "math");
  lua_pushcfunction(L, random_callback);
  lua_setfield(L, -2, "random");
  lua_pushcfunction(L, randomseed_callback);
  lua_setfield(L, -2, "randomseed");
  lua_pop(L, 1);
}
