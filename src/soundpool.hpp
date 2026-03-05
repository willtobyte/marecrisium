#pragma once

class soundpool final {
public:
  soundpool() = default;
  ~soundpool() = default;

  [[nodiscard]] sound& get(std::string_view name);

  void clear();

private:
  std::unordered_map<entt::id_type, std::unique_ptr<sound>> _pool;
};
