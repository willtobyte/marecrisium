#include "sourcepool.hpp"

namespace {
  int bytecode_writer(lua_State*, const void* data, size_t size, void* userdata) noexcept {
    auto* buffer = static_cast<std::vector<uint8_t>*>(userdata);
    const auto* bytes = static_cast<const uint8_t*>(data);
    buffer->insert(buffer->end(), bytes, bytes + size);
    return 0;
  }
}

void sourcepool::insert(std::string_view name) {
  const auto filename = std::format("objects/{}.lua", name);
  const auto key = entt::hashed_string{filename.c_str()}.value();
  const auto label = std::format("@{}", filename);

  if (const auto it = _pool.find(key); it != _pool.end()) {
    const auto& bytecode = it->second;
    const auto* data = reinterpret_cast<const char*>(bytecode.data());
    const auto size = bytecode.size();

    if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 1);
      throw std::runtime_error(std::move(error));
    }

    return;
  }

  const auto buffer = io::read(filename);
  const auto* data = reinterpret_cast<const char*>(buffer.data());
  const auto size = buffer.size();

  if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(std::move(error));
  }

  std::vector<uint8_t> bytecode;
  lua_dump(L, bytecode_writer, &bytecode);

  _pool.emplace(key, std::move(bytecode));
}

void sourcepool::clear() {
  _pool.clear();
}
