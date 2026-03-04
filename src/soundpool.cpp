#include "soundpool.hpp"

soundfx& soundpool::get(std::string_view name) {
  const auto filename = std::format("blobs/{}.opus", name);
  const auto key = entt::hashed_string{filename.c_str()}.value();
  const auto [it, result] = _pool.try_emplace(key);

  if (result)
    it->second = std::make_unique<soundfx>(filename);

  return *it->second;
}

void soundpool::clear() {
  _pool.clear();
}
