font* fontpool::get(std::string_view family) {
  if (const auto it = _pool.find(family); it != _pool.end()) [[likely]]
    return &*it->second;

  auto& instance = _pool.try_emplace(std::string{family}).first->second;
  instance.emplace(family);
  auto* result = &*instance;

  return result;
}

void fontpool::clear() {
  _pool.clear();
}
