entt::id_type stringpool::get(std::string_view value) {
  const auto key = entt::hashed_string{value.data(), value.size()};
  const auto [it, inserted] = _pool.try_emplace(key, value);
  if (inserted) [[unlikely]] {
    lua_pushlstring(L, value.data(), value.size());
    _references.emplace(key, luaL_ref(L, LUA_REGISTRYINDEX));
  }

  return key;
}

const char* stringpool::get(entt::id_type key) const {
  return _pool.find(key)->second.c_str();
}

int stringpool::reference(entt::id_type key) const {
  return _references.find(key)->second;
}
