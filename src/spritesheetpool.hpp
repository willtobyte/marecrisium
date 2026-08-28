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
  };

  std::unordered_map<std::string, std::unique_ptr<storage>, transparent_string_hash, std::equal_to<>> _pool;
};
