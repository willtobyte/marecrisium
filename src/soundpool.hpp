#pragma once

struct pcm;

class soundpool final {
public:
  const pcm* get(std::string_view name);

private:
  std::unordered_map<std::string, std::unique_ptr<pcm>, transparent_string_hash, std::equal_to<>> _pool;
};
