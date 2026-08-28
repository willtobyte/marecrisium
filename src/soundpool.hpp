#pragma once

struct audio;

class soundpool final {
public:
  const struct audio* get(std::string_view name);

private:
  std::unordered_map<std::string, std::unique_ptr<struct audio>, transparent_string_hash, std::equal_to<>> _pool;
};
