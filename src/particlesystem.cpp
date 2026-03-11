#include "particlesystem.hpp"

particle& particlesystem::add(std::string_view name, std::string_view kind, float x, float y, bool active) {
  const auto key = entt::hashed_string{name.data()}.value();
  const auto& cfg = resources.particle.get(kind);
  const auto& texture = resources.pixmap.get(std::format("particles/{}", kind));

  auto [it, inserted] = _particles.try_emplace(key, cfg, texture, x, y, active);
  return it->second;
}

void particlesystem::update(float delta) noexcept {
  for (auto& [_, p] : _particles) {
    p.update(delta);
  }
}

void particlesystem::draw(float camera_x, float camera_y) const noexcept {
  for (const auto& [_, p] : _particles) {
    p.draw(camera_x, camera_y);
  }
}

void particlesystem::clear() {
  _particles.clear();
}
