#pragma once

struct xorshift128 final {
  uint32_t state[4];

  xorshift128() = default;

  void seed(uint32_t value);

  [[nodiscard("RNG result should be used")]] uint32_t operator()();
  [[nodiscard("RNG result should be used")]] float operator()(std::pair<float, float> range);
  [[nodiscard("RNG result should be used")]] int operator()(int minimum, int maximum);

  static void wire();
};

extern xorshift128 rng;
