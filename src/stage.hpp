#pragma once

#include "math.h"
#include "minimap.hpp"
#include "particlesystem.hpp"
#include "tilemap.hpp"

class pixmap;

class stage final {
public:
  struct hit {
    entt::entity entity{entt::null};
    float fraction{};
  };

  explicit stage(std::string name);
  ~stage();

  void update(float delta);

  void draw();

  void on_enter();

  void on_leave();

  void expose();

  void conceal();

  void on_tick(uint64_t tick);

  int spawn(lua_State* state, std::string_view name, std::string_view kind, float x, float y);

  int destroy(lua_State* state);

  int count(lua_State* state);

  int find(lua_State* state);

  int radar(lua_State* state, entt::entity caller, float x, float y, float radius);

  int raycast(lua_State* state, entt::entity caller, float x, float y, float angle, float distance);

  int pathfind(lua_State* state, float x1, float y1, float x2, float y2, float radius);

  void dispatch_collision(const scriptable& self, const scriptable* target, int callback_ref, const b2Vec2* normal = nullptr);

private:
  entt::registry _registry{};
  std::string _name{};
  std::optional<std::string> _overlay{};
  std::optional<std::string> _foreground{};

  particlesystem _particlesystem{};
  tilemap _tilemap{};
  std::optional<minimap> _minimap{};
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

  std::vector<hit> _hits{};

  std::vector<SDL_Vertex> _vertices{};
  std::vector<int> _indices{};

  friend class director;

  float _sleep_margin{};
  float _wake_margin{};

  int _ref{LUA_NOREF};
  int _pool_ref{LUA_NOREF};
  int _world_ref{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_camera{LUA_NOREF};
  int _on_tick{LUA_NOREF};
  int _on_enter{LUA_NOREF};
  int _on_leave{LUA_NOREF};
};
