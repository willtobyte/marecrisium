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

  void dispatch_collision(entt::entity entity, entt::entity other, const char* callback, const b2Vec2* normal = nullptr);

protected:
  void dispatch_click(float x, float y, const char* button);

  void dispatch_press(float x, float y, const char* button);

  void dispatch_hover(float x, float y);

  void dispatch_unhover(std::span<const entt::entity> current);

  void dispatch_screen_event(const objectproxy& proxy, const char* callback, std::string_view direction);

  void dispatch_dormancy(const objectproxy& proxy, const char* callback);

  [[nodiscard]] uint8_t query_point(float x, float y, entt::entity* buffer, uint8_t capacity) const noexcept;

  [[nodiscard]] entt::entity find_topmost(std::span<const entt::entity> hits) const noexcept;

  void dispatch_miss(const char* callback, float x, float y, const char* button);

private:
  struct raycast_hit {
    entt::entity entity;
    float fraction;
  };

  entt::registry _registry;
  std::string _name;
  std::vector<sound*> _sounds;
  std::vector<entt::entity> _hovering;
  std::array<raycast_hit, 64> _raycast_hits{};
  stringpool _stringpool;
  particlesystem _particlesystem;
  const pixmap* _backdrop;
  tilemap _tilemap;
  b2WorldId _world;
  float _timestep = 1.f / 60.f;
  float _accumulator = .0f;
  uint32_t _mouse_previous_buttons{};
  int _substeps = 4;
  int _reference = LUA_NOREF;
  int _environment_reference = LUA_NOREF;
  int _pool_reference = LUA_NOREF;
  int _on_loop = LUA_NOREF;
  int _on_paint = LUA_NOREF;
  int _on_tick = LUA_NOREF;
};
