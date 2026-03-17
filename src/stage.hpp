#pragma once

#include "particlesystem.hpp"
#include "stringpool.hpp"
#include "tilemap.hpp"

class pixmap;

void sincos(float x, float& osin, float& ocos) noexcept;

class stage final {
public:
  explicit stage(std::string_view name);
  ~stage();

  void on_enter();

  void on_leave();

  void on_tick(uint64_t tick);

  void update(float delta);

  void draw();

  [[nodiscard]] int raycast(lua_State* state, entt::entity caller, float x, float y, float angle, float distance);

  [[nodiscard]] int radar(lua_State* state, float x, float y, float radius);

  void dispatch_collision(entt::entity entity, entt::entity other, const char* callback, const b2Vec2* normal = nullptr);

private:
  struct raycast_hit {
    entt::entity entity;
    float fraction;
  };

  entt::registry _registry;
  std::string _name;
  std::vector<sound*> _sounds;
  std::array<raycast_hit, 64> _raycast_hits{};
  std::array<entt::entity, 64> _radar_hits{};
  stringpool _stringpool;
  particlesystem _particlesystem;
  const pixmap* _backdrop;
  tilemap _tilemap;
  b2WorldId _world;
  float _timestep = 1.f / 60.f;
  float _accumulator = .0f;
  int _substeps = 4;
  int _reference = LUA_NOREF;
  int _environment_reference = LUA_NOREF;
  int _pool_reference = LUA_NOREF;
  int _on_loop = LUA_NOREF;
  int _on_camera = LUA_NOREF;
  int _on_tick = LUA_NOREF;
};
