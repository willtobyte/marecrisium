#pragma once

class soundpool final {
public:
  soundpool() = default;
  ~soundpool() = default;

  [[nodiscard]] soundfx& get(std::string_view name);

  void clear();

private:
  std::unordered_map<entt::id_type, std::unique_ptr<soundfx>> _pool;
};
