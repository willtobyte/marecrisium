#pragma once

struct objectproxy final {
  entt::registry* registry;
  entt::entity entity;
  int object_reference{LUA_NOREF};
  int self_reference{LUA_NOREF};

  objectproxy(entt::registry& registry, entt::entity entity, std::string_view stage, std::string_view name, int environment_reference);

  static void on_destroy(entt::registry& registry, entt::entity entity);
};
