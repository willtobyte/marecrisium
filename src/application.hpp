#pragma once

class application final {
public:
  application() = default;
  ~application() = default;

  [[nodiscard]] int run();
};
