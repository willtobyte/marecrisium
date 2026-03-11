#include "soundpool.hpp"

sound& soundpool::get(std::string_view name) {
  const auto filename = std::format("blobs/{}.opus", name);
  const auto key = entt::hashed_string{filename.c_str()}.value();
  const auto [it, inserted] = _pool.try_emplace(key);

  if (inserted)
    it->second = std::make_unique<sound>(filename);

  return *it->second;
}

void soundpool::clear() {
  _pool.clear();
}
