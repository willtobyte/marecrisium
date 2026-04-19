#pragma once

struct xorshift128 final {
  uint32_t state[4];

  xorshift128() = default;

  void seed(uint32_t value);

  uint32_t operator()();
  float operator()(std::pair<float, float> range);
  int operator()(int minimum, int maximum);

  static void wire();
};

extern xorshift128 rng;
