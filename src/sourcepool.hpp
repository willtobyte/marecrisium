#pragma once

class sourcepool final {
public:
  sourcepool() = default;
  ~sourcepool() = default;

  void insert(std::string_view name);

  void clear();

private:
  std::unordered_map<entt::id_type, std::pair<std::string, std::vector<uint8_t>>> _pool;
};
