namespace {
  static int append(lua_State*, const void* data, size_t size, void* context) {
    auto& bytecode = *static_cast<std::vector<uint8_t>*>(context);
    const auto offset = bytecode.size();
    bytecode.resize(offset + size);
    std::memcpy(bytecode.data() + offset, data, size);
    return 0;
  }
}

void sourcepool::insert(std::string_view name) {
  const auto key = entt::hashed_string{name.data(), name.size()};
  const auto filename = std::format("objects/{}.lua", name);
  const auto chunk = std::format("@{}", filename);

  const auto [it, inserted] = _pool.try_emplace(key);
  auto& bytecode = it->second;
  if (inserted) [[unlikely]] {
    const auto buffer = io::read(filename);
    if (luaL_loadbuffer(L, reinterpret_cast<const char*>(buffer.data()), buffer.size(), chunk.c_str()) != LUA_OK) [[unlikely]] {
      _pool.erase(it);
      lua_error(L);
      std::unreachable();
    }

    bytecode.reserve(buffer.size());
    const auto status = lua_dump(L, append, &bytecode);
    assert(status == 0 && "Lua bytecode dump must succeed");
    [[assume(status == 0)]];
    return;
  }

  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(bytecode.data()), bytecode.size(), chunk.c_str()) != LUA_OK) [[unlikely]] {
    lua_error(L);
    std::unreachable();
  }
}
