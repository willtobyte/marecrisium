#pragma once

template <std::size_t N>
struct literal final {
  std::array<char, N> data{};

  consteval literal(const char (&text)[N]) {
    std::ranges::copy(text, data.begin());
  }

  [[nodiscard]] constexpr std::string_view view() const noexcept {
    return {data.data(), N - 1};
  }
};

template <literal... Names>
class keyring final {
public:
  static constexpr auto size = sizeof...(Names);

  template <literal Name>
  [[nodiscard]] static consteval std::size_t id() noexcept {
    constexpr std::array<std::string_view, size> names{Names.view()...};
    constexpr auto index = static_cast<std::size_t>(std::ranges::find(names, Name.view()) - names.begin());
    static_assert(index < size, "key must be part of the keyring");

    return index;
  }

  void intern() {
    std::array<const char*, size> pointers;
    std::ranges::transform(std::array{Names.data.data()...}, pointers.begin(), [](const char* name) {
      lua_pushstring(L, name);
      const auto* pointer = lua_tostring(L, -1);
      luaL_ref(L, LUA_REGISTRYINDEX);

      return pointer;
    });

    const auto perfect = [&](std::uint64_t seed) {
      std::array<std::size_t, size> slots;
      std::ranges::transform(pointers, slots.begin(), [seed](const char* pointer) { return hash(pointer, seed); });
      std::ranges::sort(slots);

      return std::ranges::adjacent_find(slots) == slots.end();
    };

    const auto seeds = std::views::iota(1ull, 4096ull)
      | std::views::transform([](std::uint64_t n) { return n * 0x9E3779B97F4A7C15ull | 1; });
    const auto it = std::ranges::find_if(seeds, perfect);
    assert(it != seeds.end() && "keyring must find a perfect hash");
    _seed = *it;

    for (auto i = 0uz; i < size; ++i) {
      const auto slot = hash(pointers[i], _seed);
      _keys[slot] = pointers[i];
      _values[slot] = static_cast<std::uint8_t>(i);
    }
  }

  [[nodiscard]] std::size_t find(const char* key) const noexcept {
    const auto slot = hash(key, _seed);

    return _keys[slot] == key ? _values[slot] : size;
  }

private:
  static_assert(size < std::numeric_limits<std::uint8_t>::max(), "keyring must hold fewer than 255 keys");

  static constexpr auto capacity = std::bit_ceil(size * 8);
  static constexpr auto shift = 64 - std::countr_zero(capacity);

  [[nodiscard]] static std::size_t hash(const char* key, std::uint64_t seed) noexcept {
    return static_cast<std::size_t>((std::bit_cast<std::uint64_t>(key) * seed) >> shift);
  }

  std::uint64_t _seed{};
  std::array<const char*, capacity> _keys{};
  std::array<std::uint8_t, capacity> _values = [] {
    std::array<std::uint8_t, capacity> values;
    values.fill(size);

    return values;
  }();
};
