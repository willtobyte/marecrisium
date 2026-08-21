#pragma once

struct pcm final {
  explicit pcm(std::string_view filename);

  std::vector<float> samples;
  ma_uint64 length{};
  ma_uint32 channels{};
  ma_uint32 rate{};
};

class sound final {
public:
  explicit sound(const pcm& data);
  ~sound();

  void play();
  void stop();

  void set_volume(float gain);
  float volume() const;

  void set_pan(float pan);
  float pan() const;

  static void wire();

  bool playing() const;

  void fade(float from, float to, uint64_t ms);

private:
  ma_audio_buffer_ref _source{};
  ma_sound _sound{};
};
