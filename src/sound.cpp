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

namespace {
  ma_result read(ma_data_source* source, void* output, ma_uint64 frames, ma_uint64* count) {
    auto* self = reinterpret_cast<sound::stream*>(source);
    const int channels = op_channel_count(self->file, -1);

    ma_uint64 total = 0;
    while (total < frames) {
      const auto remaining = frames - total;
      const auto request = static_cast<int>(std::min<ma_uint64>(
        remaining * static_cast<ma_uint64>(channels),
        static_cast<ma_uint64>(std::numeric_limits<int>::max())
      ));

      auto* destination = static_cast<float*>(output) + total * static_cast<ma_uint64>(channels);
      const int decoded = op_read_float(self->file, destination, request, nullptr);
      if (decoded == OP_HOLE) [[unlikely]]
        continue;

      if (decoded < 0) [[unlikely]] {
        if (count) [[likely]] *count = total;
        return MA_ERROR;
      }

      if (decoded == 0) [[unlikely]]
        break;

      total += static_cast<ma_uint64>(decoded);
    }

    if (count) [[likely]] *count = total;
    if (total == 0) [[unlikely]] return MA_AT_END;
    if (total < frames) [[unlikely]] return MA_AT_END;
    return MA_SUCCESS;
  }

  ma_result seek(ma_data_source* source, ma_uint64 index) {
    auto* self = reinterpret_cast<sound::stream*>(source);
    const int result = op_pcm_seek(self->file, static_cast<ogg_int64_t>(index));
    if (result == 0) [[likely]] return MA_SUCCESS;
    if (result == OP_ENOSEEK) [[unlikely]] return MA_INVALID_OPERATION;
    return MA_ERROR;
  }

  ma_result format(ma_data_source* source, ma_format* format, ma_uint32* channels, ma_uint32* rate, ma_channel* map, size_t capacity) {
    auto* self = reinterpret_cast<sound::stream*>(source);
    const auto count = static_cast<ma_uint32>(op_channel_count(self->file, -1));
    if (format) *format = ma_format_f32;
    if (channels) *channels = count;
    if (rate) *rate = 48000;
    if (map) ma_channel_map_init_standard(ma_standard_channel_map_vorbis, map, capacity, count);
    return MA_SUCCESS;
  }

  ma_result cursor(ma_data_source* source, ma_uint64* cursor) {
    auto* self = reinterpret_cast<sound::stream*>(source);
    const auto offset = op_pcm_tell(self->file);
    if (offset < 0) [[unlikely]] return MA_ERROR;
    *cursor = static_cast<ma_uint64>(offset);
    return MA_SUCCESS;
  }

  ma_result length(ma_data_source* source, ma_uint64* length) {
    auto* self = reinterpret_cast<sound::stream*>(source);
    const auto total = op_pcm_total(self->file, -1);
    if (total < 0) [[unlikely]] return MA_ERROR;
    *length = static_cast<ma_uint64>(total);
    return MA_SUCCESS;
  }

  constexpr ma_data_source_vtable vtable = {
    read,
    seek,
    format,
    cursor,
    length,
    nullptr,
    0,
  };

  int physfs_read(void* source, unsigned char* output, int bytes) {
    return static_cast<int>(PHYSFS_readBytes(static_cast<PHYSFS_File*>(source), output, static_cast<PHYSFS_uint64>(bytes)));
  }

  int physfs_seek(void* source, opus_int64 offset, int whence) {
    auto* file = static_cast<PHYSFS_File*>(source);

    PHYSFS_sint64 base = 0;
    switch (whence) {
      case SEEK_SET: base = 0; break;
      case SEEK_CUR: base = PHYSFS_tell(file); break;
      case SEEK_END: base = PHYSFS_fileLength(file); break;
      default: return -1;
    }

    if (base < 0) [[unlikely]] return -1;

    const auto target = base + offset;
    if (target < 0) [[unlikely]] return -1;

    return PHYSFS_seek(file, static_cast<PHYSFS_uint64>(target)) != 0 ? 0 : -1;
  }

  opus_int64 physfs_tell(void* source) {
    return PHYSFS_tell(static_cast<PHYSFS_File*>(source));
  }

  constexpr OpusFileCallbacks opus_callbacks = {
    physfs_read,
    physfs_seek,
    physfs_tell,
    nullptr,
  };
}

sound::sound(std::string_view filename) {
  _file = std::unique_ptr<PHYSFS_File, PHYSFS_Deleter>{PHYSFS_openRead(filename.data())};

  _stream.file = op_open_callbacks(_file.get(), &opus_callbacks, nullptr, 0, nullptr);

  auto config = ma_data_source_config_init();
  config.vtable = &vtable;
  ma_data_source_init(&config, &_stream.base);

  ma_sound_init_from_data_source(
    &audio,
    &_stream.base,
    MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH,
    nullptr,
    &_sound
  );

  ma_sound_set_end_callback(&_sound, +[](void* self, ma_sound*) {
    static_cast<sound*>(self)->_ended.store(true, std::memory_order_release);
  }, this);
}

sound::~sound() {
  ma_sound_set_end_callback(&_sound, nullptr, nullptr);
  ma_sound_stop(&_sound);
  ma_sound_uninit(&_sound);
  ma_data_source_uninit(&_stream.base);
  op_free(_stream.file);
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
