#pragma once

class sound final {
public:
  sound() = delete;
  explicit sound(std::string_view filename);
  ~sound();

  void play();
  void stop() noexcept;

  void set_volume(float gain) noexcept;
  [[nodiscard]] float volume() const noexcept;

  void set_pan(float pan) noexcept;
  [[nodiscard]] float pan() const noexcept;

  static void wire();

  void set_loop(bool loop) noexcept;
  [[nodiscard]] bool loop() const noexcept;

  void fade(float from, float to, uint64_t ms) noexcept;

  void poll();

  int on_begin{LUA_NOREF};
  int on_end{LUA_NOREF};

private:
  std::atomic<bool> _ended{false};
  ma_audio_buffer _buffer{};
  ma_sound _sound{};
  std::unique_ptr<float[]> _samples;
};
