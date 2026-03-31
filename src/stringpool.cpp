#include "stringpool.hpp"

entt::id_type stringpool::get(std::string_view value) {
  const auto key = entt::hashed_string{value.data()};
  _pool.try_emplace(key, value);
  return key;
}

const char* stringpool::get(entt::id_type key) const noexcept {
  return _pool.find(key)->second.c_str();
}
