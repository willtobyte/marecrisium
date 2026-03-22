#include "sound.hpp"

namespace {
  int sound_play(lua_State* state) {
    auto* instance = checkuserdata<sound>(state, 1, "Sound");
    instance->play();
    fire(state, instance->on_begin);

    return 0;
  }

  int sound_stop(lua_State* state) {
    checkuserdata<sound>(state, 1, "Sound")->stop();
    return 0;
  }

  int sound_on_begin(lua_State* state) {
    auto* instance = checkuserdata<sound>(state, 1, "Sound");
    callback(state, 2, instance->on_begin);
    return 0;
  }

  int sound_fade(lua_State* state) {
    auto* instance = checkuserdata<sound>(state, 1, "Sound");
    const auto from = static_cast<float>(luaL_checknumber(state, 2));
    const auto to = static_cast<float>(luaL_checknumber(state, 3));
    const auto ms = static_cast<uint64_t>(luaL_checkinteger(state, 4));
    instance->fade(from, to, ms);
    return 0;
  }

  int sound_on_end(lua_State* state) {
    auto* instance = checkuserdata<sound>(state, 1, "Sound");
    callback(state, 2, instance->on_end);
    return 0;
  }

  int sound_index(lua_State* state) {
    auto* instance = checkuserdata<sound>(state, 1, "Sound");
    const std::string_view key = luaL_checkstring(state, 2);

    if (key == "volume")
      return push(state, instance->volume());

    if (key == "pan")
      return push(state, instance->pan());

    if (key == "loop")
      return push(state, instance->loop());

    if (key == "play") {
      lua_pushcfunction(state, sound_play);
      return 1;
    }

    if (key == "stop") {
      lua_pushcfunction(state, sound_stop);
      return 1;
    }

    if (key == "fade") {
      lua_pushcfunction(state, sound_fade);
      return 1;
    }

    if (key == "on_begin") {
      lua_pushcfunction(state, sound_on_begin);
      return 1;
    }

    if (key == "on_end") {
      lua_pushcfunction(state, sound_on_end);
      return 1;
    }

    return lua_pushnil(state), 1;
  }

  int sound_newindex(lua_State* state) {
    auto* instance = checkuserdata<sound>(state, 1, "Sound");
    const std::string_view key = luaL_checkstring(state, 2);

    if (key == "volume") {
      instance->set_volume(static_cast<float>(luaL_checknumber(state, 3)));
      return 0;
    }

    if (key == "pan") {
      instance->set_pan(static_cast<float>(luaL_checknumber(state, 3)));
      return 0;
    }

    if (key == "loop") {
      instance->set_loop(lua_toboolean(state, 3) != 0);
      return 0;
    }

    return 0;
  }
}

sound::sound(std::string_view filename) {
  const auto buffer = io::read(filename);

  const std::unique_ptr<OggOpusFile, OggOpusFile_Deleter> codec{
    op_open_memory(buffer.data(), buffer.size(), nullptr)};

  const auto channels = op_channel_count(codec.get(), -1);
  const auto nsamples = op_pcm_total(codec.get(), -1);
  const auto total = static_cast<size_t>(nsamples) * static_cast<size_t>(channels);

  _samples = std::make_unique_for_overwrite<float[]>(total);

  auto offset = 0uz;
  while (offset < total) {
    const auto read = op_read_float(
      codec.get(),
      _samples.get() + offset,
      static_cast<int>(total - offset),
      nullptr
    );

    if (read == OP_HOLE)
      continue;

    if (read < 0) [[unlikely]]
      throw std::runtime_error{std::format("[op_read_float] failed to decode: {}", filename)};

    if (read == 0)
      break;

    offset += static_cast<size_t>(read) * static_cast<size_t>(channels);
  }

  auto config = ma_audio_buffer_config_init(
    ma_format_f32,
    static_cast<ma_uint32>(channels),
    offset / static_cast<size_t>(channels),
    _samples.get(),
    nullptr
  );

  config.sampleRate = 48000;

  ma_audio_buffer_init(&config, &_buffer);

  ma_sound_init_from_data_source(
    audioengine,
    &_buffer,
    MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH,
    nullptr,
    &_sound
  );

  ma_sound_set_end_callback(&_sound, [](void* ptr, ma_sound*) {
    static_cast<sound*>(ptr)->_ended.store(true, std::memory_order_release);
  }, this);
}

sound::~sound() {
  release(L, on_begin);
  release(L, on_end);
  ma_sound_stop(&_sound);
  ma_sound_uninit(&_sound);
  ma_audio_buffer_uninit(&_buffer);
}

void sound::play() {
  ma_sound_seek_to_pcm_frame(&_sound, 0);
  ma_sound_start(&_sound);
}

void sound::stop() noexcept {
  ma_sound_stop(&_sound);
}

void sound::set_volume(float gain) noexcept {
  ma_sound_set_volume(&_sound, std::clamp(gain, .0f, 1.f));
}

float sound::volume() const noexcept {
  return ma_sound_get_volume(&_sound);
}

void sound::set_pan(float pan) noexcept {
  ma_sound_set_pan(&_sound, std::clamp(pan, -1.f, 1.f));
}

float sound::pan() const noexcept {
  return ma_sound_get_pan(&_sound);
}

void sound::set_loop(bool loop) noexcept {
  ma_sound_set_looping(&_sound, loop ? MA_TRUE : MA_FALSE);
}

bool sound::loop() const noexcept {
  return ma_sound_is_looping(&_sound) == MA_TRUE;
}

void sound::fade(float from, float to, uint64_t ms) noexcept {
  ma_sound_set_fade_in_milliseconds(&_sound, from, to, ms);
}

void sound::poll() {
  const auto ended = _ended.exchange(false, std::memory_order_acquire);
  if (ended)
    fire(L, on_end);
}

void sound::wire() {
  metatable(L, "Sound", sound_index, sound_newindex);
}
