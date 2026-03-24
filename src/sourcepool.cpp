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
  const auto key = entt::hashed_string{name.data()}.value();

  if (const auto it = _pool.find(key); it != _pool.end()) [[likely]] {
    const auto& [label, bytecode] = it->second;
    compile(L, bytecode, label);

    return;
  }

  const auto filename = std::format("objects/{}.lua", name);
  auto label = std::format("@{}", filename);

  const auto buffer = io::read(filename);
  compile(L, buffer, label);

  std::vector<uint8_t> bytecode;
  bytecode.reserve(8192);
  lua_dump(L, bytecode_writer, &bytecode);

  _pool.try_emplace(key, std::move(label), std::move(bytecode));
}

void sourcepool::clear() {
  _pool.clear();
}
