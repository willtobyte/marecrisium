#pragma once

class sound final {
public:
  explicit sound(std::string_view filename);
  ~sound();

  sound(const sound&) = delete;
  sound& operator=(const sound&) = delete;
  sound(sound&&) = delete;
  sound& operator=(sound&&) = delete;

  void play();
  void stop() noexcept;

  void set_volume(float gain) noexcept;
  [[nodiscard]] float volume() const noexcept;

  void set_loop(bool loop) noexcept;
  [[nodiscard]] bool loop() const noexcept;

  void fade(float from, float to, uint64_t ms) noexcept;

  void poll();

  int on_begin{LUA_NOREF};
  int on_end{LUA_NOREF};

private:
  [[nodiscard]] bool ended();
  ma_audio_buffer _buffer{};
  ma_sound _sound{};
  std::atomic<bool> _ended{false};
};
