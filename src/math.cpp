#include "math.hpp"

namespace {
constexpr float SIN_C0 = .99997f;
constexpr float SIN_C1 = .16596f;
constexpr float SIN_C2 = .00759f;
constexpr float COS_C0 = .99996f;
constexpr float COS_C1 = .49985f;
constexpr float COS_C2 = .03659f;

constexpr int QUADRANT_MASK = 3;
}

void sincos(float x, float& sine, float& cosine) noexcept {
  const auto raw = x * (2.f * std::numbers::inv_pi_v<float>);
  const auto q = static_cast<int>(raw) - static_cast<int>(raw < .0f);
  const auto t = x - static_cast<float>(q) * (std::numbers::pi_v<float> * .5f);
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

void sincos4(simde__m128 x, simde__m128& sine, simde__m128& cosine) noexcept {
  const auto half_pi = simde_mm_set1_ps(std::numbers::pi_v<float> * .5f);
  const auto inv_half_pi = simde_mm_set1_ps(2.f * std::numbers::inv_pi_v<float>);
  const auto sc0 = simde_mm_set1_ps(SIN_C0);
  const auto sc1 = simde_mm_set1_ps(SIN_C1);
  const auto sc2 = simde_mm_set1_ps(SIN_C2);
  const auto cc0 = simde_mm_set1_ps(COS_C0);
  const auto cc1 = simde_mm_set1_ps(COS_C1);
  const auto cc2 = simde_mm_set1_ps(COS_C2);
  const auto zero = simde_mm_setzero_ps();
  const auto one = simde_mm_set1_ps(1.f);
  const auto two = simde_mm_set1_ps(2.f);
  const auto qmask = simde_mm_set1_epi32(QUADRANT_MASK);
  const auto imask1 = simde_mm_set1_epi32(1);

  const auto raw = simde_mm_mul_ps(x, inv_half_pi);
  const auto trunc = simde_mm_cvtepi32_ps(simde_mm_cvttps_epi32(raw));
  const auto negative = simde_mm_castps_si128(simde_mm_cmplt_ps(raw, zero));
  const auto q = simde_mm_cvtps_epi32(simde_mm_sub_ps(trunc, simde_mm_and_ps(simde_mm_castsi128_ps(negative), one)));
  const auto t = simde_mm_sub_ps(x, simde_mm_mul_ps(simde_mm_cvtepi32_ps(q), half_pi));
  const auto t2 = simde_mm_mul_ps(t, t);

  const auto st = simde_mm_mul_ps(t, simde_mm_sub_ps(sc0, simde_mm_mul_ps(t2, simde_mm_sub_ps(sc1, simde_mm_mul_ps(t2, sc2)))));
  const auto ct = simde_mm_sub_ps(cc0, simde_mm_mul_ps(t2, simde_mm_sub_ps(cc1, simde_mm_mul_ps(t2, cc2))));

  const auto sq = simde_mm_and_si128(q, qmask);
  const auto swap = simde_mm_cvtepi32_ps(simde_mm_and_si128(sq, imask1));
  const auto keep = simde_mm_sub_ps(one, swap);
  const auto ss = simde_mm_sub_ps(one, simde_mm_mul_ps(two, simde_mm_cvtepi32_ps(simde_mm_and_si128(simde_mm_srli_epi32(sq, 1), imask1))));
  const auto sq1 = simde_mm_add_epi32(sq, imask1);
  const auto cs = simde_mm_sub_ps(one, simde_mm_mul_ps(two, simde_mm_cvtepi32_ps(simde_mm_and_si128(simde_mm_srli_epi32(sq1, 1), imask1))));

  sine = simde_mm_mul_ps(simde_mm_add_ps(simde_mm_mul_ps(st, keep), simde_mm_mul_ps(ct, swap)), ss);
  cosine = simde_mm_mul_ps(simde_mm_add_ps(simde_mm_mul_ps(ct, keep), simde_mm_mul_ps(st, swap)), cs);
}
