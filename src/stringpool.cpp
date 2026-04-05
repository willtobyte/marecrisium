#include "stringpool.hpp"

entt::id_type stringpool::get(std::string_view value) {
  const auto key = entt::hashed_string{value.data()};
  auto [it, inserted] = _pool.try_emplace(key, value);
  if (inserted) {
    lua_pushlstring(L, value.data(), value.size());
    _references.emplace(key, luaL_ref(L, LUA_REGISTRYINDEX));
  }
  return key;
}

const char* stringpool::get(entt::id_type key) const noexcept {
  return _pool.find(key)->second.c_str();
}

int stringpool::ref(entt::id_type key) const noexcept {
  return _references.find(key)->second;
}
