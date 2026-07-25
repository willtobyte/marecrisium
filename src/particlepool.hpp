#pragma once

class particlepool final {
public:
  particlepool() = default;
  ~particlepool() = default;

  config* get(std::string_view kind);

  void clear();

private:
  entt::dense_map<entt::id_type, std::unique_ptr<config>> _pool;
};
