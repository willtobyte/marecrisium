pixmap* pixmappool::get(std::string_view name) {
  if (const auto it = _pool.find(name); it != _pool.end()) [[likely]]
    return &it->second;

  auto* result = &_pool.emplace(name, std::format("blobs/{}.png", name)).first->second;

  return result;
}

void pixmappool::clear() {
  _pool.clear();
}
