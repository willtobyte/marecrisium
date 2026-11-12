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
    std::array<std::uint64_t, size> entries;
    auto i = 0uz;
    ((lua_pushstring(L, Names.data.data()), entries[i] = std::bit_cast<std::uint64_t>(lua_tostring(L, -1)) | i << 56, ++i, luaL_ref(L, LUA_REGISTRYINDEX)), ...);

    const auto place = [this](std::uint64_t entry) {
      auto& slot = _slots[hash(entry & mask)];

      return slot == empty && (slot = entry, true);
    };

    auto n = 0ull;
    do {
      _seed = ++n * golden | 1;
      _slots.fill(empty);
    } while (!std::ranges::all_of(entries, place));
  }

  [[nodiscard]] std::size_t find(const char* key) const noexcept {
    const auto slot = _slots[hash(std::bit_cast<std::uint64_t>(key))];

    return (slot & mask) == std::bit_cast<std::uint64_t>(key) ? slot >> 56 : size;
  }

private:
  static_assert(size <= 48, "keyring must hold at most 48 keys to find a perfect hash quickly");

  static constexpr auto capacity = std::bit_ceil(size * 4);
  static constexpr auto shift = 64 - std::countr_zero(capacity);
  static constexpr auto mask = (1ull << 56) - 1;
  static constexpr auto empty = std::uint64_t{size} << 56;
  static constexpr auto golden = 0x9E3779B97F4A7C15ull;

  [[nodiscard]] std::size_t hash(std::uint64_t key) const noexcept {
    return static_cast<std::size_t>((key * _seed) >> shift);
  }

  std::uint64_t _seed{};
  std::array<std::uint64_t, capacity> _slots{};
};
