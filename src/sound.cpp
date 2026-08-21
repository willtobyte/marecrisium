namespace {
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

pcm::pcm(std::string_view filename) {
  const auto data = io::read(filename);
  const std::unique_ptr<stb_vorbis, STB_Vorbis_Deleter> vorbis{stb_vorbis_open_memory(
    data.data(), static_cast<int>(data.size()), nullptr, nullptr)};

  const auto information = stb_vorbis_get_info(vorbis.get());
  length = stb_vorbis_stream_length_in_samples(vorbis.get());
  channels = static_cast<ma_uint32>(information.channels);
  rate = information.sample_rate;

  samples.resize(static_cast<std::size_t>(length) * channels);
  const auto decoded = stb_vorbis_get_samples_float_interleaved(
    vorbis.get(),
    static_cast<int>(channels),
    samples.data(),
    static_cast<int>(samples.size())
  );

  const auto complete = static_cast<ma_uint64>(decoded) == length;
  assert(complete && "sound must decode entirely at load time");
  [[assume(complete)]];
}

sound::sound(const pcm& data) {
  ma_audio_buffer_ref_init(ma_format_f32, data.channels, data.samples.data(), data.length, &_source);
  _source.sampleRate = data.rate;

  ma_sound_init_from_data_source(
    &audio,
    &_source,
    MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH,
    nullptr,
    &_sound
  );
}

sound::~sound() {
  ma_sound_uninit(&_sound);
  ma_audio_buffer_ref_uninit(&_source);
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
  lua_pushcfunction(L, fade_callback);
  lua_setfield(L, -2, "fade");
  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pop(L, 1);
}
