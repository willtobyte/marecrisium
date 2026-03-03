#pragma once

#include "common.hpp"

class manager;

class engine final {
public:
  engine();
  ~engine() = default;

  void run();

  void loop();

private:
  bool _running{true};
};
