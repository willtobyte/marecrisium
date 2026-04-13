#pragma once

class pixmappool final {
public:
  pixmappool() = default;
  ~pixmappool() = default;

  [[nodiscard]] pixmap* get(std::string_view name);

  void clear();

private:
  ankerl::unordered_dense::map<entt::id_type, std::unique_ptr<pixmap>> _pool;
};
