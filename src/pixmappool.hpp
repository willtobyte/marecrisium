#pragma once

class pixmappool final {
public:
  pixmap* get(std::string_view name);

  void clear();

private:
  std::unordered_map<std::string, std::unique_ptr<pixmap>, transparent_string_hash, std::equal_to<>> _pool;
};
