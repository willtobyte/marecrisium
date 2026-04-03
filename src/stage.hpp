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

  void update(float delta);

  void draw();

  void on_enter();

  void on_leave();

  void expose();

  void conceal();

  void on_tick(uint64_t tick);

  std::optional<std::string> overlay() const noexcept;

  std::optional<std::string> foreground() const noexcept;

  int spawn(lua_State* state, std::string_view name, std::string_view kind, float x, float y);

  int destroy(lua_State* state);

  int count(lua_State* state);

  int find(lua_State* state);

  int radar(lua_State* state, entt::entity caller, float x, float y, float radius);

  int raycast(lua_State* state, entt::entity caller, float x, float y, float angle, float distance);

  int pathfind(lua_State* state, float x1, float y1, float x2, float y2, float radius) noexcept;

  void dispatch_collision(entt::entity entity, entt::entity other, std::string_view callback, const b2Vec2* normal = nullptr);

private:
  entt::registry _registry{};
  std::string _name{};
  std::optional<std::string> _overlay{};
  std::optional<std::string> _foreground{};

  stringpool _stringpool{};
  particlesystem _particlesystem{};
  tilemap _tilemap{};
  std::vector<sound*> _sounds{};

  b2WorldId _world{};
  float _timestep{1.f / 60.f};
  float _accumulator{};
  int _substeps{4};

  struct {
    struct {
      float x{};
      float y{};
    } before, current, previous;
    float alpha{};
    bool ready{false};
  } _interpolation;

  struct hit {
    entt::entity entity;
    float fraction{};
  };

  std::vector<hit> _hits{};

  friend void overlap_aabb(b2WorldId, b2AABB, std::vector<hit>&) noexcept;

  float _sleep_margin{};
  float _wake_margin{};

  int _reference{LUA_NOREF};
  int _pool_reference{LUA_NOREF};
  int _world_reference{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_camera{LUA_NOREF};
  int _on_tick{LUA_NOREF};
};
