#include "sound.hpp"

namespace {
  int sound_play(lua_State* state) {
    auto** ptr = static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    auto* instance = *ptr;
    instance->play();

    if (instance->on_begin != LUA_NOREF) {
      lua_rawgeti(state, LUA_REGISTRYINDEX, instance->on_begin);

      if (lua_pcall(state, 0, 0, 0) != 0) [[unlikely]] {
        std::string error = lua_tostring(state, -1);
        lua_pop(state, 1);
        throw std::runtime_error{std::move(error)};
      }
    }

    return 0;
  }

  int sound_stop(lua_State* state) {
    auto** ptr = static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    (*ptr)->stop();
    return 0;
  }

  int sound_on_begin(lua_State* state) {
    auto** ptr = static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    auto* instance = *ptr;
    luaL_checktype(state, 2, LUA_TFUNCTION);

    luaL_unref(state, LUA_REGISTRYINDEX, instance->on_begin);

    lua_pushvalue(state, 2);
    instance->on_begin = luaL_ref(state, LUA_REGISTRYINDEX);
    return 0;
  }

  int sound_fade(lua_State* state) {
    auto** ptr = static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    const auto from = static_cast<float>(luaL_checknumber(state, 2));
    const auto to = static_cast<float>(luaL_checknumber(state, 3));
    const auto ms = static_cast<uint64_t>(luaL_checkinteger(state, 4));
    (*ptr)->fade(from, to, ms);
    return 0;
  }

  int sound_on_end(lua_State* state) {
    auto** ptr = static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    auto* instance = *ptr;
    luaL_checktype(state, 2, LUA_TFUNCTION);

    luaL_unref(state, LUA_REGISTRYINDEX, instance->on_end);

    lua_pushvalue(state, 2);
    instance->on_end = luaL_ref(state, LUA_REGISTRYINDEX);
    return 0;
  }

  int sound_index(lua_State* state) {
    auto** ptr = static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    auto* instance = *ptr;
    const std::string_view key = luaL_checkstring(state, 2);

    if (key == "volume") {
      lua_pushnumber(state, static_cast<double>(instance->volume()));
      return 1;
    }

    if (key == "loop") {
      lua_pushboolean(state, instance->loop());
      return 1;
    }

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
    auto** ptr = static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    auto* instance = *ptr;
    const std::string_view key = luaL_checkstring(state, 2);

    if (key == "volume") {
      instance->set_volume(static_cast<float>(luaL_checknumber(state, 3)));
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
  if (luaL_newmetatable(L, "Sound")) {
    lua_pushcfunction(L, sound_index);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, sound_newindex);
    lua_setfield(L, -2, "__newindex");
  }
  lua_pop(L, 1);

  const auto buffer = io::read(filename);

  auto error = 0;
  const std::unique_ptr<OggOpusFile, decltype(&op_free)> codec{
    op_open_memory(buffer.data(), buffer.size(), &error),
    &op_free
  };

  if (error != 0) [[unlikely]]
    throw std::runtime_error{std::format("[op_open_memory] failed to decode: {}", filename)};

  const auto channels = op_channel_count(codec.get(), -1);
  const auto nsamples = op_pcm_total(codec.get(), -1);
  const auto total = static_cast<size_t>(nsamples) * static_cast<size_t>(channels);

  auto samples = std::make_unique_for_overwrite<float[]>(total);

  size_t offset = 0;
  while (offset < total) {
    const auto read = op_read_float(
      codec.get(),
      samples.get() + offset,
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
    samples.get(),
    nullptr
  );

  config.sampleRate = 48000;

  ma_audio_buffer_init_copy(&config, &_buffer);

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
  luaL_unref(L, LUA_REGISTRYINDEX, on_begin);
  luaL_unref(L, LUA_REGISTRYINDEX, on_end);
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
  if (ended && on_end != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, on_end);

    if (lua_pcall(L, 0, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 1);
      throw std::runtime_error{std::move(error)};
    }
  }
}
