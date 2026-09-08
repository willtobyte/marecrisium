particleemitter* particlesystem::add(std::string_view name, std::string_view kind, float x, float y, bool active) {
  const auto it = _particles.find(name);
  if (it != _particles.end())
    return it->second.get();

  const auto* cfg = depot->get<config>(kind);
  const auto* texture = depot->get<pixmap>(std::format("particles/{}", kind));
  auto instance = std::make_unique<particleemitter>(*cfg, *texture, x, y, active);
  auto* result = instance.get();
  _particles.emplace(std::string{name}, std::move(instance));
  _order.emplace_back(result);

  return result;
}

void particlesystem::update(float delta) {
  for (auto* emitter : _order)
    emitter->update(delta);
}

void particlesystem::draw() {
  for (auto* emitter : _order)
    emitter->draw();
}

void particlesystem::clear() {
  _order.clear();
  _particles.clear();
}
