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

  [[nodiscard]] auto overlay() const noexcept -> const std::optional<std::string>&;

  void on_enter();

  void on_leave();

  void on_tick(uint64_t tick);

  void update(float delta);

  void draw();

  [[nodiscard]] int raycast(lua_State* state, entt::entity caller, float x, float y, float angle, float distance);

  [[nodiscard]] int radar(lua_State* state, entt::entity caller, float x, float y, float radius);

  [[nodiscard]] int spawn(lua_State* state, std::string_view name, std::string_view kind, float x, float y);

  int destroy(lua_State* state);

  [[nodiscard]] int pathfind(lua_State* state, float x1, float y1, float x2, float y2, float radius) noexcept;

  void dispatch_collision(entt::entity entity, entt::entity other, const char* callback, const b2Vec2* normal = nullptr);

  void dispatch_dormancy(const objectproxy& proxy, const char* callback);

  void dispatch_screen_event(const objectproxy& proxy, const char* callback, std::string_view direction);

  void dispatch_mouse_up(float x, float y, const char* button);

  void dispatch_mouse_down(float x, float y, const char* button);

  void dispatch_hover(float x, float y);

  void dispatch_unhover(std::span<const entt::entity> current);

  [[nodiscard]] uint8_t pick_at(float x, float y, entt::entity* buffer, uint8_t capacity) const noexcept;

  [[nodiscard]] entt::entity find_topmost(std::span<const entt::entity> hits) const noexcept;

  void dispatch_miss(const char* callback, float x, float y, const char* button);

private:
  struct raycast_hit {
    entt::entity entity;
    float fraction;
  };

  entt::registry _registry{};
  std::string _name{};
  std::optional<std::string> _overlay{};

  stringpool _stringpool{};
  particlesystem _particlesystem{};
  tilemap _tilemap{};
  std::vector<sound*> _sounds{};

  b2WorldId _world{};
  float _timestep{1.f / 60.f};
  float _accumulator{};
  int _substeps{4};

  float _sleep_margin{};
  float _wake_margin{};

  std::array<raycast_hit, 32> _raycast_hits{};
  std::array<entt::entity, 32> _radar_hits{};

  std::array<entt::entity, 32> _hovering{};
  uint8_t _hovering_count{};

  uint32_t _mouse_previous_buttons{};
  float _mouse_previous_x{};
  float _mouse_previous_y{};

  int _reference{LUA_NOREF};
  int _environment_reference{LUA_NOREF};
  int _pool_reference{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_camera{LUA_NOREF};
  int _on_tick{LUA_NOREF};
};
