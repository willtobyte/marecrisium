#include "math.hpp"

namespace {

constexpr float HALF_PI = 1.57079632679f;
constexpr float INV_HALF_PI = .63661977236f;

constexpr float SIN_C0 = .99997f;
constexpr float SIN_C1 = .16596f;
constexpr float SIN_C2 = .00759f;
constexpr float COS_C0 = .99996f;
constexpr float COS_C1 = .49985f;
constexpr float COS_C2 = .03659f;

constexpr int QUADRANT_MASK = 3;
constexpr float QUADRANT_SIGNS[8] = {1.f, 1.f, 1.f, -1.f, -1.f, -1.f, -1.f, 1.f};

}

void sincos(float x, float& osin, float& ocos) noexcept {
  const auto q = static_cast<int>(std::floor(x * INV_HALF_PI));
  const auto t = x - static_cast<float>(q) * HALF_PI;
  const auto t2 = t * t;

  const auto sin_t = t * (SIN_C0 - t2 * (SIN_C1 - t2 * SIN_C2));
  const auto cos_t = COS_C0 - t2 * (COS_C1 - t2 * COS_C2);

  const auto qi = (q & QUADRANT_MASK) * 2;
  const auto swap = static_cast<float>(q & 1);
  const auto keep = 1.f - swap;

  osin = (sin_t * keep + cos_t * swap) * QUADRANT_SIGNS[qi];
  ocos = (cos_t * keep + sin_t * swap) * QUADRANT_SIGNS[qi + 1];
}
