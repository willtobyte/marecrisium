#pragma once

struct proxy final {
  entt::registry* registry{};
  entt::entity entity{entt::null};
};

static_assert(std::is_trivially_copyable_v<proxy>, "proxy must be trivially copyable");

namespace object {
  void wire();
  void bind(entt::registry& registry, entt::entity entity, scriptable& component, std::string_view name, std::string_view kind);
}
