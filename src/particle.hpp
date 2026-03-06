#pragma once

class pixmap;

struct particleemitter final {
  float x{};
  float y{};
  float hw{};
  float hh{};
  bool spawning{true};
  bool shown{true};
  std::uniform_real_distribution<float> xspawnd{};
  std::uniform_real_distribution<float> yspawnd{};
  std::uniform_real_distribution<float> radiusd{};
  std::uniform_real_distribution<float> angled{};
  std::uniform_real_distribution<float> xveld{};
  std::uniform_real_distribution<float> yveld{};
  std::uniform_real_distribution<float> gxd{};
  std::uniform_real_distribution<float> gyd{};
  std::uniform_real_distribution<float> scaled{};
  std::uniform_real_distribution<float> lifed{};
  std::uniform_real_distribution<float> rotforced{};
  std::uniform_real_distribution<float> rotveld{};
};

struct particlebatch final {
  particlebatch(const pixmap& pixmap, size_t count);

  particleemitter emitter;
  const pixmap* pixmap{};
  size_t count{};
  std::vector<float> x, y, vx, vy, gx, gy;
  std::vector<float> life, scale, angle, av, af;
  std::vector<SDL_Vertex> vertices;
  std::vector<int> indices;
  std::vector<size_t> respawn;
  int handle{LUA_NOREF};
};

namespace particle {
  [[nodiscard]] std::pair<float, float> range(lua_State* state) noexcept;
  void update(particlebatch& batch, float delta);
  void draw(const particlebatch& batch) noexcept;
}
