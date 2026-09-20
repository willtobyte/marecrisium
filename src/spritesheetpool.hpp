#pragma once

class spritesheetpool final {
public:
  ~spritesheetpool();

  const spritesheet* get(std::string_view kind, lua_State* state, int index);

  void clear();

private:
  struct storage final {
    std::vector<sequence> sequences;
    std::vector<frame> frames;
    spritesheet sheet;
    std::unordered_map<std::string_view, uint8_t, transparent_string_hash, std::equal_to<>> lookup;
  };

  std::unordered_map<std::string, storage, transparent_string_hash, std::equal_to<>> _pool;
};
