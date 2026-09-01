#pragma once

class soundmanager final {
public:
  sound& add(std::string_view name);

  void dispatch();
  void stop();

private:
  static constexpr auto capacity = 16uz;
  static_assert(std::atomic_uint16_t::is_always_lock_free, "sound completion mask must be lock-free");

  std::atomic_uint16_t _completed{};
  std::uint8_t _size{};
  std::array<std::unique_ptr<sound>, capacity> _sounds{};
};
