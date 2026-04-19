#pragma once

#include "particle.hpp"

class particlepool final {
public:
  particlepool() = default;
  ~particlepool() = default;

  config* get(std::string_view kind);

  void clear();

private:
  ankerl::unordered_dense::map<entt::id_type, std::unique_ptr<config>> _pool;
};
