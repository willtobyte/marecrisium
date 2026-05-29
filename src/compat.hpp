#pragma once

#include <cstdint>

#ifdef _MSC_VER
#  include <intrin.h>
#  define noalias __restrict
#else
#  define noalias __restrict__
#endif

[[nodiscard]] inline uint64_t mix(uint64_t a, uint64_t b) noexcept {
#ifdef _MSC_VER
  uint64_t hi;
  const auto lo = _umul128(a, b, &hi);
  return lo ^ hi;
#else
  const auto r = static_cast<__uint128_t>(a) * b;
  return static_cast<uint64_t>(r) ^ static_cast<uint64_t>(r >> 64);
#endif
}
