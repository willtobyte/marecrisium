#pragma once

class fontpool final {
public:
  fontpool() = default;
  ~fontpool() = default;

  [[nodiscard]] const font& get(std::string_view family);

  void clear();

private:
  std::unordered_map<entt::id_type, font> _pool;
};
