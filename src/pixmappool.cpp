pixmap* pixmappool::get(std::string_view name) {
  if (const auto it = _pool.find(name); it != _pool.end()) [[likely]]
    return it->second.get();

  auto instance = std::make_unique<pixmap>(std::format("blobs/{}.png", name));
  auto* result = instance.get();
  _pool.emplace(name, std::move(instance));
  return result;
}

void pixmappool::clear() {
  _pool.clear();
}
