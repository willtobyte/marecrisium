#pragma once

struct objectproxy final {
  entt::registry* registry;
  entt::entity entity;
  int object_ref{LUA_NOREF};
  int self_ref{LUA_NOREF};

  objectproxy(entt::registry& registry, entt::entity entity, std::string_view stage, std::string_view name);

  static void on_destroy(entt::registry& registry, entt::entity entity);
};
