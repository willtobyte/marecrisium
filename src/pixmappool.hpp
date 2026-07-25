#pragma once

class pixmappool final {
public:
  pixmappool() = default;
  ~pixmappool() = default;

  pixmap* get(std::string_view name);

  void clear();

private:
  entt::dense_map<entt::id_type, std::unique_ptr<pixmap>> _pool;
};
