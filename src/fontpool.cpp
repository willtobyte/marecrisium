#include "fontpool.hpp"

const font& fontpool::get(std::string_view family) {
  const auto key = entt::hashed_string{family.data()}.value();
  const auto [it, _] = _pool.try_emplace(key, family);
  return it->second;
}

void fontpool::clear() {
  _pool.clear();
}
