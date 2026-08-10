#pragma once

class sound;

class soundpool final {
public:
  sound* get(std::string_view name);

private:
  std::unordered_map<std::string, std::unique_ptr<sound>, transparent_string_hash, std::equal_to<>> _pool;
};
