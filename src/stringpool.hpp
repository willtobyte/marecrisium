#pragma once

class stringpool final {
public:
  stringpool() = default;
  ~stringpool() = default;

  [[nodiscard]] entt::id_type get(std::string_view value);

  [[nodiscard]] const char* get(entt::id_type key) const noexcept;

  [[nodiscard]] int ref(entt::id_type key) const noexcept;

private:
  ankerl::unordered_dense::map<entt::id_type, std::string> _pool;
  ankerl::unordered_dense::map<entt::id_type, int> _references;
};
