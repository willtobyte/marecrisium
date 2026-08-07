#pragma once

class engine final {
public:
  engine();
  void run();

  void loop();

private:
  const std::uint64_t _boot{SDL_GetPerformanceCounter()};
  director _director;
  bool _running{true};
};
