# Rules

- **Rule Number 0: Always design with simplicity and performance in mind.**
- **Support only little-endian systems**.
- **Target idiomatic & modern C++23. Prefer C++ features such as `auto`, `constexpr`, `const`, and `final` whenever appropriate.**
- **Minimize allocations, copies, and runtime overhead.**
- **Prefer O(1), SIMD-friendly, branchless, and cache-friendly implementations whenever practical.**
- **Every C++ change to the Lua API must also update `types/game.lua`.**
- **LuaJIT only. Keep Lua code LuaJIT-friendly and performance-oriented.**
- **Every performance-related change requires empirical benchmarking. Always measure and compare the before and after results, and present the evidence demonstrating the impact.**
- **Use `assert` and `[[assume ..` whenever appropriate, always assuming the happy path.**
- **Always assume the happy path. Treat all inputs as safe and trusted.**
- **Run all benchmarks, profiling, sanitizers, fuzzing, memory-leak detection, allocation tracking, and other instrumentation on macOS 27 using Xcode Instruments and Apple Clang.**
- **All `#include` directives, of any kind, must be placed exclusively in `common.hpp` (see the current implementation), except `#include <SDL3/SDL_main.h>` in `main.cpp` and the single-header implementations in `miniaudio.cpp` and `stb.cpp`. Keep them grouped by category and sorted alphabetically. We use a precompiled header (PCH).**
- **Only create functions, methods, or abstractions if they will be used more than once. Prefer linear code whenever possible, as it is easier to read, understand, and maintain.**
- **All instrumentation-related code, tests, benchmarks, profiling tools, and similar artifacts must never be placed inside the project directory.**
