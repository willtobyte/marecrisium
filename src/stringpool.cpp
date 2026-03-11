#include "stringpool.hpp"

entt::id_type stringpool::insert(std::string_view value) {
  const auto key = entt::hashed_string{value.data()}.value();
  _pool.emplace(key, value);
  return key;
}

const char* stringpool::get(entt::id_type key) const {
  const auto it = _pool.find(key);
  return it->second.c_str();
}
