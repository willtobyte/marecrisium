#pragma once

class pixmappool final {
public:
  pixmappool() = default;
  ~pixmappool() = default;

  [[nodiscard]] pixmap& get(std::string_view name);

  void clear();

private:
  std::unordered_map<entt::id_type, pixmap> _pool;
};
