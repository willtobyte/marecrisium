#pragma once

struct clip final {
  explicit clip(std::string_view filename);

  bytes encoded;
};

struct stream final {
  ma_data_source_base source{};
  std::unique_ptr<stb_vorbis, STB_Vorbis_Deleter> vorbis;
  ma_uint64 length{};
  ma_uint32 channels{};
  ma_uint32 rate{};
};

struct completion;

class sound final {
public:
  explicit sound(const clip& data);
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
  static int on_end_callback(lua_State* state);
  static void ended(void* data, ma_sound*);
  static void SDLCALL invoke(void* data);

  stream _source{};
  ma_sound _sound{};
  std::atomic<completion*> _completion{};
};
