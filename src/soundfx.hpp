#pragma once

class soundfx final {
public:
  explicit soundfx(std::string_view filename);
  ~soundfx();

  soundfx(const soundfx&) = delete;
  soundfx& operator=(const soundfx&) = delete;
  soundfx(soundfx&&) = delete;
  soundfx& operator=(soundfx&&) = delete;

  void play();
  void stop() noexcept;

  void set_volume(float gain) noexcept;
  [[nodiscard]] float volume() const noexcept;

  void set_loop(bool loop) noexcept;
  [[nodiscard]] bool loop() const noexcept;

  void poll();

  int on_begin{LUA_NOREF};
  int on_end{LUA_NOREF};

private:
  [[nodiscard]] bool ended();
  ma_audio_buffer _buffer{};
  ma_sound _sound{};
  std::atomic<bool> _ended{false};
};

namespace sound {
  void wire();
}
