namespace {
  struct sound_completion final {
    std::atomic_uint refs{1};
    std::atomic_int callback{LUA_NOREF};
  };

  static void release(sound_completion* completion) {
    if (completion->refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
      delete completion;
  }

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
    const auto complete = stb_vorbis_seek(reinterpret_cast<stream*>(source)->vorbis.get(), static_cast<unsigned int>(frame)) != 0;
    assert(complete && "sound seek must complete");
    [[assume(complete)]];
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

  auto* completion = instance->_completion.load(std::memory_order_acquire);
  if (!completion) {
    completion = new sound_completion;
    instance->_completion.store(completion, std::memory_order_release);
  }

  lua_pushvalue(state, 2);
  const auto callback = luaL_ref(state, LUA_REGISTRYINDEX);
  const auto prior = completion->callback.exchange(callback, std::memory_order_acq_rel);
  luaL_unref(state, LUA_REGISTRYINDEX, prior);
  return 0;
}

void sound::ended(void* data, ma_sound*) {
  auto* instance = static_cast<sound*>(data);
  auto* completion = instance->_completion.load(std::memory_order_acquire);
  if (!completion)
    return;

  completion->refs.fetch_add(1, std::memory_order_relaxed);
  if (completion->callback.load(std::memory_order_acquire) == LUA_NOREF) {
    release(completion);
    return;
  }

  const auto queued = SDL_RunOnMainThread(invoke, completion, false);
  if (!queued)
    release(completion);
  assert(queued && "sound end callback must reach the main thread");
}

void SDLCALL sound::invoke(void* data) {
  auto* completion = static_cast<sound_completion*>(data);
  const auto callback = completion->callback.load(std::memory_order_acquire);

  if (!failure && callback != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
    if (pcall(L, 0, 0) != LUA_OK) [[unlikely]] {
      failure = std::make_exception_ptr(std::runtime_error{lua_tostring(L, -1)});
      lua_pop(L, 1);

      SDL_Event event{.type = SDL_EVENT_QUIT};
      const auto queued = SDL_PushEvent(&event);
      assert(queued && "sound callback error must stop the engine");
    }
  }

  release(completion);
}

clip::clip(std::string_view filename)
    : encoded{io::read(filename)} {}

sound::sound(const clip& data) {
  _source.vorbis.reset(stb_vorbis_open_memory(data.encoded.data(), static_cast<int>(data.encoded.size()), nullptr, nullptr));
  const auto valid = _source.vorbis != nullptr;
  assert(valid && "sound OGG data must be valid");
  [[assume(valid)]];

  const auto info = stb_vorbis_get_info(_source.vorbis.get());
  _source.length = stb_vorbis_stream_length_in_samples(_source.vorbis.get());
  _source.channels = static_cast<ma_uint32>(info.channels);
  _source.rate = info.sample_rate;

  auto config = ma_data_source_config_init();
  config.vtable = &vtable;
  ma_data_source_init(&config, reinterpret_cast<ma_data_source*>(&_source));

  const auto result = ma_sound_init_from_data_source(
    &audio,
    reinterpret_cast<ma_data_source*>(&_source),
    MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH,
    nullptr,
    &_sound
  );
  assert(result == MA_SUCCESS && "sound must initialize");
  [[assume(result == MA_SUCCESS)]];

  const auto callback = ma_sound_set_end_callback(&_sound, ended, this);
  assert(callback == MA_SUCCESS && "sound end callback must initialize");
  [[assume(callback == MA_SUCCESS)]];
}

sound::~sound() {
  ma_sound_uninit(&_sound);

  if (auto* completion = _completion.exchange(nullptr, std::memory_order_acq_rel)) {
    const auto callback = completion->callback.exchange(LUA_NOREF, std::memory_order_acq_rel);
    luaL_unref(L, LUA_REGISTRYINDEX, callback);
    release(completion);
  }

  ma_data_source_uninit(reinterpret_cast<ma_data_source*>(&_source));
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

bool sound::playing() const {
  return ma_sound_is_playing(&_sound) == MA_TRUE;
}

void sound::fade(float from, float to, uint64_t ms) {
  ma_sound_set_fade_in_milliseconds(&_sound, from, to, ms);
}

void sound::wire() {
  luaL_newmetatable(L, "Sound");
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
  lua_pop(L, 1);
}
