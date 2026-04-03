#include "sourcepool.hpp"

namespace {
  int bytecode_writer(lua_State*, const void* data, size_t size, void* userdata) {
    auto* buffer = static_cast<std::vector<uint8_t>*>(userdata);
    const auto* bytes = static_cast<const uint8_t*>(data);
    buffer->insert(buffer->end(), bytes, bytes + size);
    return 0;
  }
}

void sourcepool::insert(std::string_view name) {
  const auto key = entt::hashed_string{name.data()};

  if (const auto it = _pool.find(key); it != _pool.end()) [[likely]] {
    const auto& [chunk, bytecode] = it->second;
    compile(L, bytecode, chunk);

    return;
  }

  const auto filename = std::format("objects/{}.lua", name);
  auto chunk = std::format("@{}", filename);

  const auto buffer = io::read(filename);
  compile(L, buffer, chunk);

  std::vector<uint8_t> bytecode;
  bytecode.reserve(8192);
  lua_dump(L, bytecode_writer, &bytecode);

  _pool.try_emplace(key, std::move(chunk), std::move(bytecode));
}

void sourcepool::clear() {
  _pool.clear();
}
