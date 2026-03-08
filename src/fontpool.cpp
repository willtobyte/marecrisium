#include "fontpool.hpp"

const font& fontpool::get(std::string_view family) {
  const auto filename = std::format("overlay/fonts/{}", family);
  const auto key = entt::hashed_string{filename.c_str()}.value();
  const auto [it, _] = _pool.try_emplace(key, family);
  return it->second;
}

void fontpool::clear() {
  _pool.clear();
}
