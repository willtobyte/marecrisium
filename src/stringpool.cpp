#include "stringpool.hpp"

void stringpool::insert(entt::id_type key, std::string_view value) {
  _pool.emplace(key, value);
}

const char* stringpool::get(entt::id_type key) const {
  return _pool.at(key).c_str();
}
