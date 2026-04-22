particle* particlesystem::add(std::string_view name, std::string_view kind, float x, float y, bool active) {
  const auto key = entt::hashed_string{name.data(), name.size()};
  const auto *config = depot->particle.get(kind);
  const auto *texture = depot->pixmap.get(std::format("particles/{}", kind));

  auto [it, inserted] = _particles.try_emplace(key, std::make_unique<particle>(*config, *texture, x, y, active));
  return it->second.get();
}

void particlesystem::update(float delta) {
  for (auto& [_, p] : _particles) {
    p->update(delta);
  }
}

void particlesystem::draw() {
  for (auto& [_, p] : _particles) {
    p->draw();
  }
}

void particlesystem::clear() {
  _particles.clear();
}
