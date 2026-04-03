#pragma once

class stringpool final {
public:
  stringpool() = default;
  ~stringpool() = default;

  entt::id_type get(std::string_view value);

  const char* get(entt::id_type key) const noexcept;

private:
  ankerl::unordered_dense::map<entt::id_type, std::string> _pool;
};
