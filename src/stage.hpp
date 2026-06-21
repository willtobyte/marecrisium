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

  void dispatch_collision(const scriptable& self, const scriptable* target, int callback_reference, const b2Vec2* normal = nullptr);

  [[nodiscard]] uint8_t pick_at(float x, float y, entt::entity* buffer, uint8_t capacity) const noexcept;

  [[nodiscard]] entt::entity find_topmost(std::span<const entt::entity> hits) const noexcept;

  void dispatch_press(float x, float y, const char* button);

  void dispatch_release(float x, float y, const char* button);

  void dispatch_hover(float x, float y);

  void dispatch_unhover(std::span<const entt::entity> current);

  void dispatch_miss(int callback_reference, float x, float y, const char* button);

private:
  entt::registry _registry{};
  std::string _name{};
  std::vector<std::string> _foregrounds{};

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
  std::vector<entt::entity> _pending{};

  std::vector<SDL_Vertex> _vertices{};
  std::vector<int> _indices{};

  friend class director;

  float _sleep_margin{};
  float _wake_margin{};

  int _reference{LUA_NOREF};
  int _pool_reference{LUA_NOREF};
  int _world_reference{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_camera{LUA_NOREF};
  int _on_tick{LUA_NOREF};
  int _on_enter{LUA_NOREF};
  int _on_leave{LUA_NOREF};
  int _on_press{LUA_NOREF};
  int _on_release{LUA_NOREF};

  std::vector<entt::entity> _hovering{};
  uint32_t _mouse_previous_buttons{};

#ifdef DEBUG
  b2Vec2 _origin{};
  b2Vec2 _target{};
  b2Vec2 _center{};
  float _radius{};
#endif
};
