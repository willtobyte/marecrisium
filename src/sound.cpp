namespace {
  static ma_result read(ma_data_source* source, void* frames, ma_uint64 count, ma_uint64* decoded) {
    auto* stream = reinterpret_cast<struct stream*>(source);
    const auto limit = static_cast<ma_uint64>(std::numeric_limits<int>::max() / stream->channels);
    auto total = ma_uint64{};

    while (total < count) {
      const auto size = std::min(count - total, limit);
      const auto result = stb_vorbis_get_samples_float_interleaved(
        stream->vorbis.get(),
        static_cast<int>(stream->channels),
        static_cast<float*>(frames) + total * stream->channels,
        static_cast<int>(size * stream->channels)
      );
      total += static_cast<ma_uint64>(result);
      if (result < size)
        break;
    }

    *decoded = total;
    return total == 0 ? MA_AT_END : MA_SUCCESS;
  }

  static ma_result seek(ma_data_source* source, ma_uint64 frame) {
    stb_vorbis_seek(reinterpret_cast<stream*>(source)->vorbis.get(), static_cast<unsigned int>(frame));

    return MA_SUCCESS;
  }

  static ma_result format(ma_data_source* source, ma_format* kind, ma_uint32* channels, ma_uint32* rate, ma_channel* map, size_t capacity) {
    const auto* stream = reinterpret_cast<struct stream*>(source);
    if (kind)
      *kind = ma_format_f32;
    if (channels)
      *channels = stream->channels;
    if (rate)
      *rate = stream->rate;
    if (map)
      ma_channel_map_init_standard(ma_standard_channel_map_vorbis, map, capacity, stream->channels);

    return MA_SUCCESS;
  }

  static ma_result cursor(ma_data_source* source, ma_uint64* frame) {
    *frame = static_cast<ma_uint64>(stb_vorbis_get_sample_offset(reinterpret_cast<stream*>(source)->vorbis.get()));
    return MA_SUCCESS;
  }

  static ma_result length(ma_data_source* source, ma_uint64* frame) {
    *frame = reinterpret_cast<stream*>(source)->length;
    return MA_SUCCESS;
  }

  constexpr ma_data_source_vtable vtable{
    .onRead = read,
    .onSeek = seek,
    .onGetDataFormat = format,
    .onGetCursor = cursor,
    .onGetLength = length,
  };

  static sound* get(lua_State* state) {
    return *static_cast<sound**>(luaL_checkudata(state, 1, "Sound"));
  }

  static int play_callback(lua_State* state) {
    auto* instance = get(state);
    instance->play();

    return 0;
  }

  static int stop_callback(lua_State* state) {
    auto* instance = get(state);
    instance->stop();

    return 0;
  }

  static int fade_callback(lua_State* state) {
    auto* instance = get(state);
    const auto from = std::clamp(static_cast<float>(luaL_checknumber(state, 2)), -1.f, 1.f);
    const auto to = std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 1.f);
    const auto duration = std::clamp(
      luaL_checkinteger(state, 4),
      lua_Integer{},
      static_cast<lua_Integer>(std::numeric_limits<uint32_t>::max()));
    instance->fade(from, to, static_cast<uint64_t>(duration));

    return 0;
  }

  static int index(lua_State* state) {
    auto* instance = get(state);
    std::size_t length;
    const auto* data = luaL_checklstring(state, 2, &length);
    const std::string_view key{data, length};

    if (key == "volume") {
      lua_pushnumber(state, static_cast<lua_Number>(instance->volume()));

      return 1;
    }

    if (key == "pan") {
      lua_pushnumber(state, static_cast<lua_Number>(instance->pan()));

      return 1;
    }

    if (key == "playing") {
      lua_pushboolean(state, instance->playing());

      return 1;
    }

    lua_getmetatable(state, 1);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    return 1;
  }

  static int newindex(lua_State* state) {
    auto* instance = get(state);
    std::size_t length;
    const auto* data = luaL_checklstring(state, 2, &length);
    const std::string_view key{data, length};

    if (key == "volume")
      instance->set_volume(static_cast<float>(luaL_checknumber(state, 3)));
    else if (key == "pan")
      instance->set_pan(static_cast<float>(luaL_checknumber(state, 3)));

    return 0;
  }
}

int sound::on_end_callback(lua_State* state) {
  auto* instance = get(state);
  luaL_checktype(state, 2, LUA_TFUNCTION);

  lua_pushvalue(state, 2);
  instance->_callback = luaL_ref(state, LUA_REGISTRYINDEX);

  lua_pushvalue(state, 1);
  instance->_self = luaL_ref(state, LUA_REGISTRYINDEX);

  return 0;
}

void sound::ended(void* data, ma_sound*) noexcept {
  auto* const instance = static_cast<sound*>(data);
  auto state = phase::armed;
  if (!instance->_phase.compare_exchange_strong(
        state,
        phase::publishing,
        std::memory_order_acq_rel,
        std::memory_order_relaxed))
    return;

  instance->_completed->fetch_or(instance->_bit, std::memory_order_release);
  instance->_phase.store(phase::idle, std::memory_order_release);
}

clip::clip(std::string_view filename) {
  const auto source = io::read(filename);
  encoded.assign(source.data(), source.data() + source.size());
}

sound::sound(const clip& data, std::atomic_uint16_t& completed, std::uint16_t bit)
    : _completed{&completed}, _bit{bit} {
  _source.vorbis.reset(stb_vorbis_open_memory(data.encoded.data(), static_cast<int>(data.encoded.size()), nullptr, nullptr));

  const auto info = stb_vorbis_get_info(_source.vorbis.get());
  _source.length = stb_vorbis_stream_length_in_samples(_source.vorbis.get());
  _source.channels = static_cast<ma_uint32>(info.channels);
  _source.rate = info.sample_rate;

  auto config = ma_data_source_config_init();
  config.vtable = &vtable;
  ma_data_source_init(&config, reinterpret_cast<ma_data_source*>(&_source));

  ma_sound_init_from_data_source(
    &audio,
    reinterpret_cast<ma_data_source*>(&_source),
    MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH,
    nullptr,
    &_sound
  );

  ma_sound_set_end_callback(&_sound, ended, this);
}

sound::~sound() {
  ma_sound_uninit(&_sound);

  luaL_unref(L, LUA_REGISTRYINDEX, _callback);
  luaL_unref(L, LUA_REGISTRYINDEX, _self);

  ma_data_source_uninit(reinterpret_cast<ma_data_source*>(&_source));
}

void sound::play() {
  if (_phase.load(std::memory_order_acquire) == phase::idle &&
      (_completed->load(std::memory_order_relaxed) & _bit) == 0) [[likely]] {
    _phase.store(phase::armed, std::memory_order_release);
    ma_sound_seek_to_pcm_frame(&_sound, 0);
    ma_sound_start(&_sound);

    return;
  }

  stop();
  _phase.store(phase::armed, std::memory_order_release);
  ma_sound_seek_to_pcm_frame(&_sound, 0);
  ma_sound_start(&_sound);
}

void sound::stop() {
  const auto active = _phase.load(std::memory_order_acquire) != phase::idle;
  ma_sound_stop(&_sound);
  if (active) {
    ma_node_detach_output_bus(&_sound, 0);

    _phase.store(phase::idle, std::memory_order_release);

    ma_node_attach_output_bus(&_sound, 0, ma_node_graph_get_endpoint(ma_engine_get_node_graph(&audio)), 0);
  }

  if ((_completed->load(std::memory_order_relaxed) & _bit) != 0)
    _completed->fetch_and(static_cast<std::uint16_t>(~_bit), std::memory_order_acq_rel);
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

bool sound::playing() const {
  return ma_sound_is_playing(&_sound) == MA_TRUE;
}

void sound::fade(float from, float to, uint64_t ms) {
  ma_sound_set_fade_in_milliseconds(&_sound, from, to, ms);
}

void sound::wire() {
  lua_createtable(L, 0, 7);
  lua_pushliteral(L, "Sound");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, play_callback);
  lua_setfield(L, -2, "play");
  lua_pushcfunction(L, stop_callback);
  lua_setfield(L, -2, "stop");
  lua_pushcfunction(L, on_end_callback);
  lua_setfield(L, -2, "on_end");
  lua_pushcfunction(L, fade_callback);
  lua_setfield(L, -2, "fade");
  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_setfield(L, LUA_REGISTRYINDEX, "Sound");
}
