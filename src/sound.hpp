#pragma once

class sound final {
public:
  sound() = delete;
  explicit sound(std::string_view filename);
  ~sound();

  void play();
  void stop();

  void set_volume(float gain);
  [[nodiscard]] float volume() const;

  void set_pan(float pan);
  [[nodiscard]] float pan() const;

  static void wire();

  void set_loop(bool loop);
  [[nodiscard]] bool loop() const;

  [[nodiscard]] bool playing() const;

  void fade(float from, float to, uint64_t ms);

  void poll();

  int on_begin{LUA_NOREF};
  int on_end{LUA_NOREF};

private:
  std::atomic<bool> _ended{false};
  ma_audio_buffer _buffer{};
  ma_sound _sound{};
  std::unique_ptr<float[]> _samples;
};
