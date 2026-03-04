#include "pixmappool.hpp"

const pixmap& pixmappool::get(std::string_view stage, std::string_view name) {
  const auto key = entt::hashed_string{name.data()}.value();
  const auto [it, _] = _pool.try_emplace(key, std::format("blobs/{}/{}.png", stage, name));
  return it->second;
}
