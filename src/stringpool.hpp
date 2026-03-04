#pragma once

class stringpool final {
public:
  stringpool() = default;
  ~stringpool() = default;

  entt::id_type insert(std::string_view value);

  [[nodiscard]] const char* get(entt::id_type key) const;

private:
  std::unordered_map<entt::id_type, std::string> _pool;
};
