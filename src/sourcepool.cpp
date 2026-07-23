namespace {
  static int writer(lua_State*, const void* data, size_t size, void* userdata) {
    auto* buffer = static_cast<std::vector<uint8_t>*>(userdata);
    const auto* bytes = static_cast<const uint8_t*>(data);
    buffer->insert(buffer->end(), bytes, bytes + size);
    return 0;
  }
}

void sourcepool::insert(std::string_view name) {
  const auto key = entt::hashed_string{name.data(), name.size()};

  if (const auto it = _pool.find(key); it != _pool.end()) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, it->second.reference);
    return;
  }

  const auto filename = std::format("objects/{}.lua", name);
  auto chunk = std::format("@{}", filename);

  const auto buffer = io::read(filename);
  binding::load(L, buffer, chunk);

  std::vector<uint8_t> bytecode;
  bytecode.reserve(8192);
  lua_dump(L, writer, &bytecode);

  lua_pushvalue(L, -1);
  const auto reference = luaL_ref(L, LUA_REGISTRYINDEX);

  _pool.try_emplace(key, source{std::move(chunk), std::move(bytecode), reference});
}

void sourcepool::clear() {
  for (auto& [_, e] : _pool) {
    luaL_unref(L, LUA_REGISTRYINDEX, e.reference);
    e.reference = LUA_NOREF;
  }

  _pool.clear();
}
