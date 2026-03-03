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
};
