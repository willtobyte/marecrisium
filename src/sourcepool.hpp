#pragma once

class sourcepool final {
public:
  sourcepool() = default;
  ~sourcepool() = default;

  void load(std::string_view stage, std::string_view name);

  void push(std::string_view stage, std::string_view name) const;

private:
  std::unordered_map<entt::id_type, std::vector<uint8_t>> _pool;
};
