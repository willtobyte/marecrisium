#pragma once

struct objectproxy final {
  entt::registry* registry;
  entt::entity entity;
  entt::id_type name{};
  entt::id_type kind{};
  int prototype{LUA_NOREF};
  int handle{LUA_NOREF};

  objectproxy(
    entt::registry& registry, entt::entity entity,
    std::string_view name, std::string_view kind,
    int environment
  );

  static void on_destroy(entt::registry& registry, entt::entity entity);
};
