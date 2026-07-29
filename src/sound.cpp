namespace {
  namespace lookup {
    constexpr auto volume = "volume"_hs;
    constexpr auto pan = "pan"_hs;
    constexpr auto loop = "loop"_hs;
    constexpr auto playing = "playing"_hs;
  }

  static int play_callback(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    instance->play();
    if (instance->on_begin != LUA_NOREF) {
      lua_rawgeti(state, LUA_REGISTRYINDEX, instance->on_begin);
      lua_call(state, 0, 0);
    }

    return 0;
  }

  static int stop_callback(lua_State* state) {
    (*static_cast<sound**>(luaL_checkudata(state, 1, "Sound")))->stop();
    return 0;
  }

  static int on_begin_callback(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    luaL_unref(state, LUA_REGISTRYINDEX, instance->on_begin);
    instance->on_begin = LUA_NOREF;
    lua_pushvalue(state, 2);
    instance->on_begin = luaL_ref(state, LUA_REGISTRYINDEX);
    return 0;
  }

  static int fade_callback(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    const auto from = std::clamp(static_cast<float>(luaL_checknumber(state, 2)), .0f, 1.f);
    const auto to = std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 1.f);
    const auto duration = std::clamp(
      luaL_checkinteger(state, 4),
      lua_Integer{},
      static_cast<lua_Integer>(std::numeric_limits<uint32_t>::max()));
    instance->fade(from, to, static_cast<uint64_t>(duration));
    return 0;
  }

  static int on_end_callback(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    luaL_unref(state, LUA_REGISTRYINDEX, instance->on_end);
    instance->on_end = LUA_NOREF;
    lua_pushvalue(state, 2);
    instance->on_end = luaL_ref(state, LUA_REGISTRYINDEX);
    return 0;
  }

  static int index(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

    switch (id) {
      case lookup::volume:
        lua_pushnumber(state, static_cast<lua_Number>(instance->volume()));
        return 1;

      case lookup::pan:
        lua_pushnumber(state, static_cast<lua_Number>(instance->pan()));
        return 1;

      case lookup::loop:
        lua_pushboolean(state, instance->loop());
        return 1;

      case lookup::playing:
        lua_pushboolean(state, instance->playing());
        return 1;

      default:
        lua_getmetatable(state, 1);
        lua_pushvalue(state, 2);
        lua_rawget(state, -2);
        return 1;
    }
  }

  static int newindex(lua_State* state) {
    auto* instance = *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
    const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

    switch (id) {
      case lookup::volume:
        instance->set_volume(static_cast<float>(luaL_checknumber(state, 3)));
        return 0;

      case lookup::pan:
        instance->set_pan(static_cast<float>(luaL_checknumber(state, 3)));
        return 0;

      case lookup::loop:
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
    if (rate) *rate = 48'000;
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

}

sound::sound(std::string_view filename)
  : _data{io::read(filename)} {
  _opus.reset(op_open_memory(_data.data(), _data.size(), nullptr));
  _stream.file = _opus.get();

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
  {
    const auto base = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 0, 0, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }
}

void sound::wire() {
  luaL_newmetatable(L, "Sound");
  lua_pushliteral(L, "Sound");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, play_callback);
  lua_setfield(L, -2, "play");
  lua_pushcfunction(L, stop_callback);
  lua_setfield(L, -2, "stop");
  lua_pushcfunction(L, fade_callback);
  lua_setfield(L, -2, "fade");
  lua_pushcfunction(L, on_begin_callback);
  lua_setfield(L, -2, "on_begin");
  lua_pushcfunction(L, on_end_callback);
  lua_setfield(L, -2, "on_end");
  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pop(L, 1);
}
