xorshift128 prng{};

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

uint32_t xorshift128::next() {
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

float xorshift128::operator()(std::pair<float, float> range) {
  constexpr auto scale = 0x1.fffffep-33f;
  const auto [minimum, maximum] = range;
  return minimum + (maximum - minimum) * (static_cast<float>(next()) * scale);
}

int xorshift128::operator()(int minimum, int maximum) {
  assert(minimum <= maximum && "random range must be ordered");
  [[assume(minimum <= maximum)]];

  const auto range = static_cast<uint32_t>(maximum) - static_cast<uint32_t>(minimum) + 1u;
  auto draw = next();
  if (range == 0) [[unlikely]]
    return static_cast<int>(static_cast<int64_t>(minimum) + draw);

  auto product = static_cast<uint64_t>(draw) * static_cast<uint64_t>(range);
  auto low = static_cast<uint32_t>(product);
  if (low < range) [[unlikely]] {
    const auto threshold = (-range) % range;
    while (low < threshold) [[unlikely]] {
      draw = next();
      product = static_cast<uint64_t>(draw) * static_cast<uint64_t>(range);
      low = static_cast<uint32_t>(product);
    }
  }

  return static_cast<int>(static_cast<int64_t>(minimum) + static_cast<int64_t>(product >> 32));
}
