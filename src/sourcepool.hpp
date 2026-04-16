#pragma once

class sourcepool final {
public:
  sourcepool() = default;
  ~sourcepool() = default;

  void insert(std::string_view name);

  void clear();

private:
  struct source final {
    std::string chunk;
    std::vector<uint8_t> bytecode;
    int reference{LUA_NOREF};
  };

  ankerl::unordered_dense::map<entt::id_type, source> _pool;
};
