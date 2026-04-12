#pragma once

struct xorshift128 final {
  uint32_t state[4];

  xorshift128() noexcept = default;

  void seed(uint32_t value) noexcept;

  [[nodiscard]] uint32_t operator()() noexcept;
  [[nodiscard]] float operator()(std::pair<float, float> range) noexcept;
  [[nodiscard]] int operator()(int minimum, int maximum) noexcept;

  static void wire();
};

extern xorshift128 rng;
