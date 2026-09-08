font* fontpool::get(std::string_view family) {
  if (const auto it = _pool.find(family); it != _pool.end()) [[likely]]
    return it->second.get();

  auto instance = std::make_unique<font>(family);
  auto* result = instance.get();
  _pool.emplace(family, std::move(instance));

  return result;
}

void fontpool::clear() {
  _pool.clear();
}
