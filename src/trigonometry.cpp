namespace {
constexpr auto crossing = .7035f;
constexpr auto curvature = -.814f;
}

void sincos(float angle, float& sine, float& cosine) noexcept {
  const auto raw = angle * (2.f * std::numbers::inv_pi_v<float>);
  const auto quadrant = static_cast<int>(raw) - static_cast<int>(raw < .0f);
  const auto reduced = raw - quadrant - .5f;
  const auto shared = curvature * reduced * reduced + crossing;

  auto sin = std::bit_cast<std::uint32_t>(shared + reduced);
  auto cos = std::bit_cast<std::uint32_t>(shared - reduced);

  const auto index = quadrant & 3;
  const auto selection = 0u - static_cast<std::uint32_t>(index & 1);
  const auto difference = (sin ^ cos) & selection;

  sin ^= difference;
  cos ^= difference;

  const auto ss = static_cast<std::uint32_t>(index & 2) << 30;
  const auto cs = static_cast<std::uint32_t>((index + 1) & 2) << 30;

  sine = std::bit_cast<float>(sin ^ ss);
  cosine = std::bit_cast<float>(cos ^ cs);
}
