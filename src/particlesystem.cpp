particle* particlesystem::add(std::string_view name, std::string_view kind, float x, float y, bool active) {
  const auto key = entt::hashed_string{name.data(), name.size()};
  if (const auto it = _particles.find(key); it != _particles.end())
    return it->second.get();

  const auto *cfg = depot->get<config>(kind);
  const auto *texture = depot->get<pixmap>(std::format("particles/{}", kind));
  auto instance = std::make_unique<particle>(*cfg, *texture, x, y, active);
  return _particles.emplace(key, std::move(instance)).first->second.get();
}

void particlesystem::update(float delta) {
  for (auto&& [_, p] : _particles) {
    p->update(delta);
  }
}

void particlesystem::draw() {
  for (auto&& [_, p] : _particles) {
    p->draw();
  }
}

void particlesystem::clear() {
  _particles.clear();
}
