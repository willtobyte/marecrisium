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

class sound final {
public:
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
  enum class phase : std::uint16_t {
    idle,
    armed,
    publishing,
  };
  static_assert(std::atomic<phase>::is_always_lock_free, "sound completion state must be lock-free");

  sound(const clip& data, std::atomic_uint16_t& completed, std::uint16_t bit);

  static int on_end_callback(lua_State* state);
  static void ended(void* data, ma_sound*) noexcept;

  stream _source{};
  ma_sound _sound{};
  std::atomic_uint16_t* _completed{};
  int _callback{LUA_NOREF};
  int _self{LUA_NOREF};
  std::atomic<phase> _phase{};
  std::uint16_t _bit{};

  friend class soundmanager;
};
