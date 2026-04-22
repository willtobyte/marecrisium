sound* soundpool::get(std::string_view name) {
  const auto key = entt::hashed_string{name.data(), name.size()};
  const auto [it, inserted] = _pool.try_emplace(key, nullptr);
  if (inserted) [[unlikely]]
    it->second = std::make_unique<sound>(std::format("blobs/{}.opus", name));

  return it->second.get();
}

void soundpool::clear() {
  _pool.clear();
}
