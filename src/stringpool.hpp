#pragma once

class stringpool final {
public:
  entt::id_type get(std::string_view value);

  const char* get(entt::id_type key) const;

  int reference(entt::id_type key) const;

private:
  entt::dense_map<entt::id_type, std::string> _pool;
  entt::dense_map<entt::id_type, int> _references;
};
