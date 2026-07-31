#pragma once

struct mcg64 final {
  uint64_t state;

  mcg64() { seed(std::random_device{}()); }

  constexpr void seed(uint32_t value) noexcept {
    constexpr auto increment = uint64_t{0x9E3779B97F4A7C15};
    constexpr auto first = uint64_t{0xBF58476D1CE4E5B9};
    constexpr auto second = uint64_t{0x94D049BB133111EB};

    auto z = static_cast<uint64_t>(value) + increment;
    z = (z ^ (z >> 30)) * first;
    z = (z ^ (z >> 27)) * second;
    state = (z ^ (z >> 31)) | 1ull;
  }

  constexpr float operator()(std::pair<float, float> range) noexcept {
    constexpr auto scale = 0x1.fffffep-33f;
    const auto [minimum, maximum] = range;
    return minimum + (maximum - minimum) * (static_cast<float>(next()) * scale);
  }

private:
  constexpr uint32_t next() noexcept {
    constexpr auto multiplier = uint64_t{0xD1342543DE82EF95};

    state *= multiplier;
    return static_cast<uint32_t>(state >> 32);
  }
};
