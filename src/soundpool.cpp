#include "soundpool.hpp"

sound& soundpool::get(std::string_view name) {
  const auto key = entt::hashed_string{name.data()}.value();
  const auto it = _pool.find(key);
  if (it != _pool.end()) [[likely]]
    return it->second;

  return _pool.try_emplace(key, std::format("blobs/{}.opus", name)).first->second;
}

void soundpool::clear() {
  _pool.clear();
}
