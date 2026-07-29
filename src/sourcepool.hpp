#pragma once

class sourcepool final {
public:
  void insert(std::string_view name);

private:
  entt::dense_map<entt::id_type, std::vector<uint8_t>> _pool;
};
