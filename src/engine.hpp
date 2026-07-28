#pragma once

class engine final {
public:
  engine();
  void run();

  void loop();

private:
  bool _running{true};
  director _director;
};
