# Rules

- **Always design with simplicity and performance in mind.**
- **Support only little-endian systems**.
- **Target idiomatic & modern C++23. Prefer C++ features such as `auto`, `constexpr`, `const`, and `final` whenever appropriate.**
- **Minimize allocations, copies, and runtime overhead.**
- **Prefer O(1), SIMD-friendly, branchless, and cache-friendly implementations whenever practical.**
- **Every C++ change to the Lua API must also update `types/game.lua`.**
- **LuaJIT only. Keep Lua code LuaJIT-friendly and performance-oriented.**
- **Every performance-related change requires empirical benchmarking. Always measure and compare the before and after results, and present the evidence demonstrating the impact.**
- **Run all benchmarks, profiling, sanitizers, fuzzing, memory-leak detection, allocation tracking, and other instrumentation on macOS 26 using Xcode Instruments and Apple Clang.**
- **Use `assert` and `[[assume ..` whenever appropriate, always assuming the happy path.**
- **Always assume the happy path. Treat all inputs as safe and trusted.**
