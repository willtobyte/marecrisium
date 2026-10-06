#pragma once

namespace mouse {
  enum event : uint8_t {
    hover,
    unhover,
    click,
  };

  void wire();
}
