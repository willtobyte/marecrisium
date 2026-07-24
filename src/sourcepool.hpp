#pragma once

class sourcepool final {
public:
  sourcepool() = default;
  ~sourcepool() = default;

  void insert(std::string_view name);
};
