#pragma once

class fontpool final {
public:
  font* get(std::string_view family);

  void clear();

private:
  std::unordered_map<std::string, std::optional<font>, transparent_string_hash, std::equal_to<>> _pool;
};
