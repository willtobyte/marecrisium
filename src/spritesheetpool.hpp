#pragma once

#include "spritesheet.hpp"

class spritesheetpool final {
public:
  spritesheetpool() = default;
  ~spritesheetpool() = default;

  const spritesheet* get(std::string_view kind, lua_State* state, int index);

  void clear();

private:
  struct storage {
    std::vector<clip> clips;
    std::vector<frame> frames;
    spritesheet sheet;
  };

  ankerl::unordered_dense::map<entt::id_type, std::unique_ptr<storage>> _pool;
};
