#pragma once

class soundpool final {
public:
  soundpool() = default;
  ~soundpool() = default;

  sound* get(std::string_view name);

  void clear();

private:
  ankerl::unordered_dense::map<entt::id_type, std::unique_ptr<sound>> _pool;
};
