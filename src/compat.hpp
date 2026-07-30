#pragma once

#ifdef _MSC_VER
#  define noalias __restrict
#else
#  define noalias __restrict__
#endif

[[nodiscard]] inline uint64_t mix(uint64_t left, uint64_t right) noexcept {
#ifdef _MSC_VER
  const auto lo = left * right;
  const auto hi = __umulh(left, right);
  return lo ^ hi;
#else
  const auto product = static_cast<__uint128_t>(left) * right;
  return static_cast<uint64_t>(product) ^ static_cast<uint64_t>(product >> 64);
#endif
}
