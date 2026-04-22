font* fontpool::get(std::string_view family) {
  const auto key = entt::hashed_string{family.data(), family.size()};
  const auto [it, inserted] = _pool.try_emplace(key, nullptr);
  if (inserted) [[unlikely]]
    it->second = std::make_unique<font>(family);

  return it->second.get();
}

void fontpool::clear() {
  _pool.clear();
}
