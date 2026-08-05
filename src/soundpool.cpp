sound* soundpool::get(std::string_view name) {
  const auto key = entt::hashed_string{name.data(), name.size()};
  const auto [it, inserted] = _pool.try_emplace(key, nullptr);
  if (inserted) [[unlikely]]
    it->second = std::make_unique<sound>(std::format("blobs/{}.ogg", name));

  return it->second.get();
}

void soundpool::poll() {
  for (auto&& [_, instance] : _pool) {
    if (instance->playing())
      instance->poll();
  }
}

void soundpool::clear() {
  _pool.clear();
}
