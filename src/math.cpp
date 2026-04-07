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
}

void sincos(float x, float& sine, float& cosine) noexcept {
  const auto raw = x * INV_HALF_PI;
  const auto q = static_cast<int>(raw) - static_cast<int>(raw < .0f);
  const auto t = x - static_cast<float>(q) * HALF_PI;
  const auto t2 = t * t;

  const auto st = t * (SIN_C0 - t2 * (SIN_C1 - t2 * SIN_C2));
  const auto ct = COS_C0 - t2 * (COS_C1 - t2 * COS_C2);

  const auto sq = q & QUADRANT_MASK;
  const auto swap = static_cast<float>(sq & 1);
  const auto keep = 1.f - swap;
  const auto ss = 1.f - 2.f * static_cast<float>((sq >> 1) & 1);
  const auto cs = 1.f - 2.f * static_cast<float>(((sq + 1) >> 1) & 1);

  sine = (st * keep + ct * swap) * ss;
  cosine = (ct * keep + st * swap) * cs;
}
