#include "stringpool.hpp"

entt::id_type stringpool::insert(std::string_view value) {
  const auto key = entt::hashed_string{value.data()}.value();
  _pool.emplace(key, value);
  return key;
}

const char* stringpool::get(entt::id_type key) const noexcept {
  return _pool.find(key)->second.c_str();
}
