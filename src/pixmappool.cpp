void pixmappool::load(std::string_view stage, std::string_view name) {
  const auto filename = std::format("blobs/{}/{}.png", stage, name);
  const auto key = entt::hashed_string{name.data()}.value();
  _pool.try_emplace(key, filename);
}

const pixmap& pixmappool::get(entt::id_type key) const {
  const auto it = _pool.find(key);
  if (it == _pool.end()) [[unlikely]]
    throw std::runtime_error("pixmappool: pixmap not found");
  return it->second;
}
