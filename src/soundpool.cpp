#include "soundpool.hpp"

soundfx& soundpool::get(std::string_view stage, std::string_view name) {
  const auto key = entt::hashed_string{name.data()}.value();
  const auto [it, result] = _pool.try_emplace(key);

  if (result)
    it->second = std::make_unique<soundfx>(std::format("blobs/{}/{}.opus", stage, name));

  return *it->second;
}
