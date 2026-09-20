# Rules

* Never create Git branches or commits.
* Never edit generated files in cartridge/objects/ directly; use the object's patch.lua.j2.
* Keep tests, benchmarks, fuzzers, profiling tools, and other instrumentation outside the project directory.
* Write simple, readable code. Assume trusted inputs and the happy path; avoid routine defensive checks.
* Support little-endian systems only, including Apple Silicon M1+, ARM64, and Intel CPUs released within the last 10 years.
* Support LuaJIT only; keep Lua code LuaJIT-friendly and performance-oriented.
* Update types/carimbo.lua whenever C++ changes the Lua API.
* Use the project's pcall() wrapper for every Lua function call, never lua_pcall() or lua_call(). Check every call status and propagate all Lua errors to the catch block in src/application.cpp.
* Create Lua tables in C++ with lua_createtable, passing known array and hash sizes or zero for unknown sizes.
* Intern frequently pushed LuaJIT strings, but not one-off strings.
* Use assert and [[assume(...)]] only when necessary to protect memory, resource lifetime, or data integrity—not for routine checks of trusted inputs. Every assert must include a short, clear English error message.
* Fuzz all newly written code.
* Benchmark every performance-related change. Measure before and after, report the comparison, and reject non-marginal regressions.
* Use a high-precision clock for benchmarks, never FPS. Measure release builds with NOVSYNC=1 and fullscreen enabled; never use WINDOWED=1.
* Run all benchmarks, profiling, sanitizers, fuzzing, leak detection, allocation tracking, and other instrumentation on macOS 27 with Xcode Instruments and Apple Clang.
* Run Linux AMD64 and Windows AMD64 tests on the remote Docker host at 92.112.178.107 using SSH and DOCKER_HOST. Run Windows tests under Wine64 in Linux containers.
* Use local Docker through OrbStack only for Linux ARM64 or when the remote host is unavailable.
* Remove all unused Docker data before every Docker command.
* Use modern, idiomatic C++23, preferring auto, constexpr, const, and final where appropriate.
* Minimize allocations, copies, and runtime overhead. Prefer O(1), SIMD-friendly, branchless, and cache-friendly implementations when practical.
* Use SIMDe for all explicit SIMD operations.
* Review struct and class member order for efficient memory layout and cache use.
* Use the project precompiled header. Keep all #include directives in common.hpp, except SDL3/SDL_main.h in main.cpp and single-header implementation includes in miniaudio.cpp and stb.cpp.
* Group includes by category and sort alphabetically within each group.
* Create functions, methods, and abstractions only when used more than once.
* Prefer clear single-word variable names and unambiguous local abbreviations.
* Add a blank line before return only when another statement precedes it in the same block; never when return is the first statement in a block or file.
* Reject performance regressions, unless they are marginal.
