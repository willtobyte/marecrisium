# Rules

## General

* Never create Git branches or commits.
* Never edit generated files in cartridge/objects/ directly; use the object's object.lua.j2.
* The engine has one developer, who knows its contracts and controls its code, scripts, and data. Fully trust all content in `cartridge.rom`, including file names and the existence of referenced files.
* Always assume the happy path. Always assume that all input is correct and that all state is initialized. Do not add defensive checks, validation, or fallback paths for hypothetical misuse.
* **Always use the happy path.** Use it to make the code:
  * **Branchless:** remove all branches, checks, and error paths that the happy path makes unnecessary.
  * **Allocation-free:** decrease memory allocations to the minimum. Prefer zero allocations on hot paths.
  * **Copy-free:** decrease copies to the minimum. Prefer views, references, and in-place operations.
  * **Fast:** decrease runtime overhead, instructions, and cache misses.
  * **Simple:** write the shortest and most direct code that works for correct input.

## Platforms

* Support only Windows AMD64 and macOS Apple Silicon (M1 or later). Use little-endian systems only. For Windows, target AMD64 CPUs released within the last 10 years.
* Build and run macOS Apple Silicon binaries natively with Apple Clang.
* On macOS, run everything in the background. Run in the foreground only when the user asks for it.
* Use only the `Sandbox` CrossOver bottle to build Windows AMD64 binaries from source and run them. If `Sandbox` does not exist, create it and install the required build and runtime tools. Never create or use other bottles.

## Steam

* If a task needs Steam for Windows, you can use Steam for Windows AMD64 on the `Steam` CrossOver bottle. Use it with extreme caution and do not abuse it. Avoid a ban or suspicious activity at all costs.
* If a task needs Steam for macOS, you can use the Steam application that is already installed & signed in. Use it with extreme caution and do not abuse it. Avoid a ban or suspicious activity at all costs.

## C++

* Use modern, idiomatic C++23, preferring auto, constexpr, const, and final where appropriate.
* Use the project precompiled header. Keep all #include directives in common.hpp, except SDL3/SDL_main.h in main.cpp and single-header implementation includes in miniaudio.cpp and stb.cpp.
* Group includes by category and sort alphabetically within each group.
* Use assert and [[assume(...)]] only when necessary to protect memory, resource lifetime, or data integrity—not for routine checks of trusted inputs. Every assert must include a short, clear English error message.
* Create helpers and abstractions only when they provide a clear benefit. Reuse alone is not a reason to create a helper. Helpers such as `checkerror` that only check a status and throw an error are forbidden. Repetition is acceptable and preferred when it keeps the code clear and direct.
* Prefer clear single-word variable names and unambiguous local abbreviations.
* Add a blank line before return only when another statement precedes it in the same block; never when return is the first statement in a block or file.

## Lua

* Support LuaJIT only; keep Lua code LuaJIT-friendly and performance-oriented.
* Update types/carimbo.lua whenever C++ changes the Lua API.
* Use the project's pcall() wrapper for every Lua function call, never lua_pcall() or lua_call(). Check every call status and propagate all Lua errors to the catch block in src/application.cpp.
* Create Lua tables in C++ with lua_createtable, passing known array and hash sizes or zero for unknown sizes.
* Intern frequently pushed LuaJIT strings, but not one-off strings.

## Performance

* Minimize allocations, copies, and runtime overhead. Prefer O(1) and branchless implementations when practical.
* All code must be both SIMD-friendly and cache-friendly. Both requirements are mandatory when practical.
* Use SIMDe for all explicit SIMD operations.
* Review struct and class member order for efficient memory layout and cache use.

## Testing and Fuzzing

* Keep tests, benchmarks, fuzzers, profiling tools, and other instrumentation outside the project directory.
* Fuzz all newly written code.

## Benchmarks

* Run all benchmarks on release builds only, on all platforms. Never benchmark debug builds.
* Benchmark every performance-related change. Measure time, memory allocations, and copies before and after. Report the comparison and reject non-marginal regressions in each metric.
* Never use frames per second (FPS) or any metric derived from it. Do not measure the whole frame. Measure only the code that the change affects, with high-precision clocks, to prevent noise from unrelated work. Report the median, p99, and maximum time, not only the average.

### Linux AMD64

* To measure the performance of a code block, ideally use a container on OrbStack with Linux AMD64. Linux AMD64 has much more instrumentation than macOS and CrossOver: `perf stat` and `perf record` for cycles and counters, Valgrind `callgrind` and `cachegrind` for instructions and cache misses, `massif` and `heaptrack` for allocations, `gprof`, and Last Branch Record. Build a release binary with the same flags as the shipped build and no instrumentation. Report time, cycles, cache misses, and allocations for the code block before and after the change. Keep the macOS native and `Sandbox` CrossOver measurements for the final binaries of each platform.

### macOS Apple Silicon

* For macOS binaries, run benchmarks, profiling, sanitizers, fuzzing, leak detection, allocation tracking, and other instrumentation natively on macOS 27 Apple Silicon with Apple Clang and Xcode Instruments.

### Windows AMD64

* For Windows AMD64 binaries, run benchmarks only in the `Sandbox` CrossOver bottle on macOS 27 Apple Silicon. Build with Microsoft Visual C++ (MSVC), Windows SDK, CMake, Ninja, and Conan. Measure time with `QueryPerformanceCounter` and `QueryPerformanceFrequency`. Measure allocations in separate builds with mimalloc compiled with `MI_STAT=2` and a LuaJIT allocator wrapper set with `lua_setallocf`. Count copies and moves in test builds outside the project directory.
* For Windows profiling, use Tracy Profiler with `TRACY_ENABLE`, `TRACY_NO_SYSTEM_TRACING`, and `TRACY_TIMER_QPC`. Use `tracy-capture` to save traces and `tracy-csvexport` to inspect the recorded data. Record CPU zones and memory allocations in instrumented builds outside the project directory. Use `TracyAlloc` and `TracyFree` to track allocations and releases.
* Run Windows instrumentation only in the `Sandbox` CrossOver bottle. Verify that each tool can collect valid data in CrossOver before using its results. Report unsupported tools and unavailable measurements.
