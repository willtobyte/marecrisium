#pragma once

struct objectproxy final {
  entt::registry* registry;
  entt::entity entity;
  entt::id_type name{};
  entt::id_type kind{};
  int prototype{LUA_NOREF};
  int handle{LUA_NOREF};
  int on_loop{LUA_NOREF};
  int on_animation_end{LUA_NOREF};
  int on_animation_begin{LUA_NOREF};
  int on_collision_begin{LUA_NOREF};
  int on_collision_end{LUA_NOREF};
  int on_wake{LUA_NOREF};
  int on_sleep{LUA_NOREF};
  int on_screen_exit{LUA_NOREF};
  int on_screen_enter{LUA_NOREF};
  int on_spawn{LUA_NOREF};

  objectproxy(entt::registry& registry, entt::entity entity, std::string_view name, std::string_view kind);
};

namespace object {
  void wire();
}
