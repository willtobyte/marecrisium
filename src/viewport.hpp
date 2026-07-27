#pragma once

struct viewport final {
  float width;
  float height;
  float scale;
  float x;
  float y;

  constexpr bool operator==(const viewport&) const = default;
};
