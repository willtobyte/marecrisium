#pragma once

class pixmappool final {
public:
  pixmappool() = default;
  ~pixmappool() = default;

  [[nodiscard]] const pixmap& get(std::string_view stage, std::string_view name);

private:
  std::unordered_map<entt::id_type, pixmap> _pool;
};
