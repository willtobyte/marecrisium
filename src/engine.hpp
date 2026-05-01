#pragma once

class engine final {
public:
  engine();
  ~engine() = default;

  void run();

  void loop();

private:
  bool _running{true};
  uint64_t _tick{0};
  float _period{.0f};
  float _accumulator{.0f};

  director _director;
};
