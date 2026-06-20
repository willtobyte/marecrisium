#pragma once

class sound final {
public:
  struct stream final {
    ma_data_source_base base{};
    OggOpusFile* file{nullptr};
  };

  sound() = delete;
  explicit sound(std::string_view filename);
  ~sound();

  void play();
  void stop();

  void set_volume(float gain);
  float volume() const;

  void set_pan(float pan);
  float pan() const;

  static void wire();

  void set_loop(bool loop);
  bool loop() const;

  bool playing() const;

  void fade(float from, float to, uint64_t ms);

  void poll();

  int on_begin{LUA_NOREF};
  int on_end{LUA_NOREF};

private:
  std::atomic<bool> _ended{false};
  std::unique_ptr<PHYSFS_File, PHYSFS_Deleter> _file{};
  stream _stream{};
  ma_sound _sound{};
};
