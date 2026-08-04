std::size_t sourcepool::hash::operator()(std::string_view value) const noexcept {
  return entt::hashed_string{value.data(), value.size()}.value();
}

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
  const auto filename = std::format("objects/{}.lua", name);
  const auto chunk = std::format("@{}", filename);

  if (const auto found = _pool.find(name); found != _pool.end()) [[likely]] {
    const auto& bytecode = found->second;
    if (luaL_loadbuffer(L, reinterpret_cast<const char*>(bytecode.data()), bytecode.size(), chunk.c_str()) != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
    return;
  }

  const auto [it, inserted] = _pool.try_emplace(std::string{name});
  assert(inserted && "source must not already be cached");
  [[assume(inserted)]];
  auto& bytecode = it->second;
  const auto buffer = io::read(filename);
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(buffer.data()), buffer.size(), chunk.c_str()) != LUA_OK) [[unlikely]] {
    _pool.erase(it);
    lua_error(L);
    std::unreachable();
  }

  bytecode.reserve(buffer.size());
  const auto status = lua_dump(L, append, &bytecode);
  assert(status == LUA_OK && "Lua bytecode dump must succeed");
  [[assume(status == LUA_OK)]];
}
