#pragma once

void sincos(float x, float& sine, float& cosine);

template <typename T>
[[nodiscard("Angle conversion has no side effects")]]
constexpr T to_radians(T degrees) {
  if consteval {
    return degrees * (std::numbers::pi_v<T> / T{180});
  } else {
    static constinit auto factor = std::numbers::pi_v<T> / T{180};
    return degrees * factor;
  }
}
