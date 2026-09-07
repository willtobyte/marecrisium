playback::playback() noexcept = default;

playback::~playback() = default;

sound& playback::add(std::string_view name) {
  const auto available = _size < capacity;
  assert(available && "scene must not exceed 16 sounds");
  [[assume(available)]];

  const auto key = std::format("sounds/{}", name);
  const auto* data = depot->get<clip>(key);
  const auto bit = static_cast<std::uint16_t>(1u << _size);
  auto instance = std::unique_ptr<sound>{new sound{*data, _completed, bit}};
  auto& result = *instance;
  _sounds[_size++] = std::move(instance);
  return result;
}

void playback::update() {
  if (_completed.load(std::memory_order_relaxed) == 0) [[likely]]
    return;

  auto bits = _completed.exchange(0, std::memory_order_acquire);
  while (bits) {
    const auto index = static_cast<std::size_t>(std::countr_zero(bits));
    bits &= static_cast<std::uint16_t>(bits - 1);
    auto& sound = *_sounds[index];
    if (sound._phase.load(std::memory_order_acquire) != sound::phase::armed)
      ma_sound_stop(&sound._sound);
    if (sound._callback == LUA_NOREF)
      continue;

    lua_rawgeti(L, LUA_REGISTRYINDEX, sound._callback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, sound._self);
    if (pcall(L, 1, 0) != LUA_OK) [[unlikely]]
      propagate();
  }
}

void playback::stop() {
  for (auto i = std::uint8_t{}; i < _size; ++i)
    _sounds[i]->stop();

  _completed.store(0, std::memory_order_relaxed);
}
