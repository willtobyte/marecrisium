#pragma once

struct audio final {
  explicit audio(std::string_view filename);

  bytes encoded;
};

struct stream final {
  ma_data_source_base source{};
  std::unique_ptr<stb_vorbis, STB_Vorbis_Deleter> vorbis;
  ma_uint64 length{};
  ma_uint32 channels{};
  ma_uint32 rate{};
};

class sound final {
public:
  explicit sound(const struct audio& data);
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
  stream _source{};
  ma_sound _sound{};
};
