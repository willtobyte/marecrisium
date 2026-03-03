namespace {
  int bytecode_writer(lua_State*, const void* data, size_t size, void* ud) noexcept {
    auto* buffer = static_cast<std::vector<uint8_t>*>(ud);
    const auto* bytes = static_cast<const uint8_t*>(data);
    buffer->insert(buffer->end(), bytes, bytes + size);
    return 0;
  }
}

void sourcepool::load(std::string_view stage, std::string_view name) {
  const auto filename = std::format("objects/{}/{}.lua", stage, name);
  const auto key = entt::hashed_string{filename.c_str()}.value();

  if (_pool.contains(key))
    return;

  const auto buffer = io::read(filename);
  const auto* data = reinterpret_cast<const char*>(buffer.data());
  const auto size = buffer.size();
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(error);
  }

  std::vector<uint8_t> bytecode;
  lua_dump(L, bytecode_writer, &bytecode);
  lua_pop(L, 1);

  _pool.emplace(key, std::move(bytecode));
}

void sourcepool::push(std::string_view stage, std::string_view name) const {
  const auto filename = std::format("objects/{}/{}.lua", stage, name);
  const auto key = entt::hashed_string{filename.c_str()}.value();
  const auto label = std::format("@{}", filename);

  const auto it = _pool.find(key);
  if (it == _pool.end()) [[unlikely]]
    throw std::runtime_error(std::format("sourcepool: source not found for {}", filename));

  const auto& bytecode = it->second;
  const auto* data = reinterpret_cast<const char*>(bytecode.data());
  const auto size = bytecode.size();

  if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(error);
  }
}
