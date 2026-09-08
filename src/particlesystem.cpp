particleemitter* particlesystem::add(std::string_view name, std::string_view kind, float x, float y, bool active) {
  const auto it = _particles.find(name);
  if (it != _particles.end())
    return &it->second;

  const auto* cfg = depot->get<config>(kind);
  const auto* texture = depot->get<pixmap>(std::format("particles/{}", kind));
  _indices.reserve(std::max(_indices.size(), cfg->count * 6));
  for (auto i = _indices.size() / 6; i < cfg->count; ++i) {
    const auto base = static_cast<int>(i * 4);
    _indices.insert(_indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  }

  auto* result = &_particles.try_emplace(std::string{name}, *cfg, *texture, x, y, active).first->second;
  _order.emplace_back(result);

  return result;
}

void particlesystem::update(float delta) {
  for (auto* emitter : _order)
    emitter->update(delta);
}

void particlesystem::draw() {
  for (auto* emitter : _order)
    emitter->draw(_indices.data());
}

void particlesystem::clear() {
  _order.clear();
  _particles.clear();
}
