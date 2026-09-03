#pragma once

struct viewport final {
  float width;
  float height;
  float scale;

  constexpr bool operator==(const viewport&) const = default;
};
