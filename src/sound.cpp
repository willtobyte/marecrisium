#include "sound.hpp"

namespace {
  namespace property {
    constexpr auto volume = "volume"_hs;
    constexpr auto pan = "pan"_hs;
    constexpr auto loop = "loop"_hs;
    constexpr auto playing = "playing"_hs;
    constexpr auto play = "play"_hs;
    constexpr auto stop = "stop"_hs;
    constexpr auto fade = "fade"_hs;
    constexpr auto on_begin = "on_begin"_hs;
    constexpr auto on_end = "on_end"_hs;
  }

  static int sound_play(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    instance->play();
    if (instance->on_begin != LUA_NOREF) {
      lua_rawgeti(state, LUA_REGISTRYINDEX, instance->on_begin);
      pcall(state, 0, 0);
    }

    return 0;
  }

  static int sound_stop(lua_State* state) {
    (*static_cast<sound**>(luaL_checkudata(state, 1, "Sound")))->stop();
    return 0;
  }

  static int sound_on_begin(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    luaL_checktype(state, 2, LUA_TFUNCTION);
    luaL_unref(state, LUA_REGISTRYINDEX, instance->on_begin);
    instance->on_begin = LUA_NOREF;
    lua_pushvalue(state, 2);
    instance->on_begin = luaL_ref(state, LUA_REGISTRYINDEX);
    return 0;
  }

  static int sound_fade(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    const auto from = static_cast<float>(luaL_checknumber(state, 2));
    const auto to = static_cast<float>(luaL_checknumber(state, 3));
    const auto ms = static_cast<uint64_t>(std::max(luaL_checkinteger(state, 4), lua_Integer{0}));
    instance->fade(from, to, ms);
    return 0;
  }

  static int sound_on_end(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    luaL_checktype(state, 2, LUA_TFUNCTION);
    luaL_unref(state, LUA_REGISTRYINDEX, instance->on_end);
    instance->on_end = LUA_NOREF;
    lua_pushvalue(state, 2);
    instance->on_end = luaL_ref(state, LUA_REGISTRYINDEX);
    return 0;
  }

  int _play_ref = LUA_NOREF;
  int _stop_ref = LUA_NOREF;
  int _fade_ref = LUA_NOREF;
  int _on_begin_ref = LUA_NOREF;
  int _on_end_ref = LUA_NOREF;

  static int sound_index(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

    switch (id) {
      case property::volume:
        lua_pushnumber(state, static_cast<lua_Number>(instance->volume()));
        return 1;

      case property::pan:
        lua_pushnumber(state, static_cast<lua_Number>(instance->pan()));
        return 1;

      case property::loop:
        lua_pushboolean(state, instance->loop() ? 1 : 0);
        return 1;

      case property::playing:
        lua_pushboolean(state, instance->playing() ? 1 : 0);
        return 1;

      case property::play:
        lua_rawgeti(state, LUA_REGISTRYINDEX, _play_ref);
        return 1;

      case property::stop:
        lua_rawgeti(state, LUA_REGISTRYINDEX, _stop_ref);
        return 1;

      case property::fade:
        lua_rawgeti(state, LUA_REGISTRYINDEX, _fade_ref);
        return 1;

      case property::on_begin:
        lua_rawgeti(state, LUA_REGISTRYINDEX, _on_begin_ref);
        return 1;

      case property::on_end:
        lua_rawgeti(state, LUA_REGISTRYINDEX, _on_end_ref);
        return 1;

      default:
        return lua_pushnil(state), 1;
    }
  }

  static int sound_newindex(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

    switch (id) {
      case property::volume:
        instance->set_volume(static_cast<float>(luaL_checknumber(state, 3)));
        return 0;

      case property::pan:
        instance->set_pan(static_cast<float>(luaL_checknumber(state, 3)));
        return 0;

      case property::loop:
        instance->set_loop(lua_toboolean(state, 3) != 0);
        return 0;

      default:
        return 0;
    }
  }
}

sound::sound(std::string_view filename) {
  const auto buffer = io::read(filename);

  const std::unique_ptr<OggOpusFile, OggOpusFile_Deleter> codec{
    op_open_memory(buffer.data(), buffer.size(), nullptr)};

  const auto channels = op_channel_count(codec.get(), -1);
  const auto samples = op_pcm_total(codec.get(), -1);
  const auto total = static_cast<size_t>(samples) * static_cast<size_t>(channels);

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

    assert(read >= 0 && "[op_read_float] failed to decode");

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

  ma_sound_set_end_callback(&_sound, +[](void* ptr, ma_sound*) {
    static_cast<sound*>(ptr)->_ended.store(true, std::memory_order_release);
  }, this);
}

sound::~sound() {
  ma_sound_set_end_callback(&_sound, nullptr, nullptr);
  ma_sound_stop(&_sound);
  ma_sound_uninit(&_sound);
  ma_audio_buffer_uninit(&_buffer);
  luaL_unref(L, LUA_REGISTRYINDEX, on_begin);
  luaL_unref(L, LUA_REGISTRYINDEX, on_end);
}

void sound::play() {
  ma_sound_seek_to_pcm_frame(&_sound, 0);
  ma_sound_start(&_sound);
}

void sound::stop() {
  ma_sound_stop(&_sound);
}

void sound::set_volume(float gain) {
  ma_sound_set_volume(&_sound, std::clamp(gain, .0f, 1.f));
}

float sound::volume() const {
  return ma_sound_get_volume(&_sound);
}

void sound::set_pan(float pan) {
  ma_sound_set_pan(&_sound, std::clamp(pan, -1.f, 1.f));
}

float sound::pan() const {
  return ma_sound_get_pan(&_sound);
}

void sound::set_loop(bool loop) {
  ma_sound_set_looping(&_sound, loop ? MA_TRUE : MA_FALSE);
}

bool sound::loop() const {
  return ma_sound_is_looping(&_sound) == MA_TRUE;
}

bool sound::playing() const {
  return ma_sound_is_playing(&_sound) == MA_TRUE;
}

void sound::fade(float from, float to, uint64_t ms) {
  ma_sound_set_fade_in_milliseconds(&_sound, from, to, ms);
}

void sound::poll() {
  if (!_ended.exchange(false, std::memory_order_acquire))
    return;

  if (on_end == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, on_end);
  pcall(L, 0, 0);
}

void sound::wire() {
  lua_pushcfunction(L, sound_play);
  _play_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushcfunction(L, sound_stop);
  _stop_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushcfunction(L, sound_fade);
  _fade_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushcfunction(L, sound_on_begin);
  _on_begin_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushcfunction(L, sound_on_end);
  _on_end_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  metatable(L, "Sound", sound_index, sound_newindex);
}
