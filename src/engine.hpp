#pragma once

class engine final {
public:
  engine();
  void run();

  void loop();

private:
  director _director;
  bool _running{true};
};
