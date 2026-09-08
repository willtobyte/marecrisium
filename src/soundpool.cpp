const clip* soundpool::get(std::string_view name) {
  if (const auto it = _pool.find(name); it != _pool.end()) [[likely]]
    return it->second.get();

  auto instance = std::make_unique<clip>(std::format("blobs/{}.ogg", name));
  auto* result = instance.get();
  _pool.emplace(name, std::move(instance));

  return result;
}
