#pragma once

struct clip;

class soundpool final {
public:
  const clip* get(std::string_view name);

private:
  std::unordered_map<std::string, clip, transparent_string_hash, std::equal_to<>> _pool;
};
