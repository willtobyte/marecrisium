#pragma once

class pixmappool final {
public:
  pixmappool() = default;
  ~pixmappool() = default;

  void load(std::string_view stage, std::string_view name);

  [[nodiscard]] const pixmap& get(entt::id_type key) const;

private:
  std::unordered_map<entt::id_type, pixmap> _pool;
};
