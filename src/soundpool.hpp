#pragma once

class soundpool final {
public:
  sound* get(std::string_view name);

  void clear();

private:
  entt::dense_map<entt::id_type, std::unique_ptr<sound>> _pool;
};
