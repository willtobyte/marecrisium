namespace {
consteval float sin_c0() { return .99997f; }
consteval float sin_c1() { return .16596f; }
consteval float sin_c2() { return .00759f; }
consteval float cos_c0() { return .99996f; }
consteval float cos_c1() { return .49985f; }
consteval float cos_c2() { return .03659f; }

constexpr int QUADRANT_MASK = 3;
}

void sincos(float x, float& sine, float& cosine) {
  const auto raw = x * (2.f * std::numbers::inv_pi_v<float>);
  const auto q = static_cast<int>(raw) - static_cast<int>(raw < .0f);
  const auto t = x - static_cast<float>(q) * (std::numbers::pi_v<float> * .5f);
  const auto t2 = t * t;

  const auto st = t * (sin_c0() - t2 * (sin_c1() - t2 * sin_c2()));
  const auto ct = cos_c0() - t2 * (cos_c1() - t2 * cos_c2());

  const auto sq = q & QUADRANT_MASK;
  const auto swap = static_cast<float>(sq & 1);
  const auto keep = 1.f - swap;
  const auto ss = 1.f - 2.f * static_cast<float>((sq >> 1) & 1);
  const auto cs = 1.f - 2.f * static_cast<float>(((sq + 1) >> 1) & 1);

  sine = (st * keep + ct * swap) * ss;
  cosine = (ct * keep + st * swap) * cs;
}
