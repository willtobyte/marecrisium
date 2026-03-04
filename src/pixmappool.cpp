#include "pixmappool.hpp"

const pixmap& pixmappool::get(std::string_view name) {
  const auto filename = std::format("blobs/{}.png", name);
  const auto key = entt::hashed_string{filename.c_str()}.value();
  const auto [it, _] = _pool.try_emplace(key, filename);
  return it->second;
}
