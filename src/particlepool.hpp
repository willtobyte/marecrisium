#pragma once

class particlepool final {
public:
  config* get(std::string_view kind);

  void clear();

private:
  std::unordered_map<std::string, config, transparent_string_hash, std::equal_to<>> _pool;
};
