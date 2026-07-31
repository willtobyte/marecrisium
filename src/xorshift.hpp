#pragma once

struct xorshift128 final {
  uint32_t state[4];

  xorshift128() { seed(std::random_device{}()); }

  void seed(uint32_t value);

  float operator()(std::pair<float, float> range);
  int operator()(int minimum, int maximum);

private:
  uint32_t next();
};
