#pragma once

class fontpool final {
public:
  fontpool() = default;
  ~fontpool() = default;

  font* get(std::string_view family);

  void clear();

private:
  ankerl::unordered_dense::map<entt::id_type, std::unique_ptr<font>> _pool;
};
