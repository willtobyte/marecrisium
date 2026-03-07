#pragma once

class engine final {
public:
  engine();
  ~engine() = default;

  void run();

  void loop();

private:
  bool _running{true};
  director _director;
  uint64_t _tick{0};
  float _tick_interval{.0f};
  float _tick_accumulator{.0f};
};
