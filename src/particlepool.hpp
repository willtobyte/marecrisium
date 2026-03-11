#pragma once

#include "particle.hpp"

class particlepool final {
public:
  particlepool() = default;
  ~particlepool() = default;

  [[nodiscard]] config& get(std::string_view kind);

  void clear();

private:
  std::unordered_map<entt::id_type, config> _pool;
};
