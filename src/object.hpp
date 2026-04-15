#pragma once

struct scriptable final {
  static constexpr auto in_place_delete = true;

  entt::registry* registry;
  entt::entity entity;
  entt::id_type name{};
  entt::id_type kind{};

  int prototype{LUA_NOREF};
  int handle{LUA_NOREF};
  int name_ref{LUA_NOREF};
  int kind_ref{LUA_NOREF};
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
};

static_assert(std::is_trivially_copyable_v<scriptable>);

namespace object {
  void wire();
  void bind(scriptable& proxy, std::string_view name, std::string_view kind);
}
