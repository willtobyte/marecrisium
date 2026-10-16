# Rules

* Never create Git branches or commits.
* Never edit generated files in cartridge/objects/ directly; use the object's patch.lua.j2.
* Keep tests, benchmarks, fuzzers, profiling tools, and other instrumentation outside the project directory.
* The engine has one developer, who knows its contracts and controls its code, scripts, and data. Fully trust all content in `cartridge.rom`, including file names and the existence of referenced files. Always assume the happy path: correct use, valid inputs, and initialized state. Write simple, readable code. Do not add defensive checks or fallback paths for hypothetical misuse.
* Support only Windows AMD64 and macOS Apple Silicon (M1 or later). Use little-endian systems only. For Windows, target AMD64 CPUs released within the last 10 years.
* Support LuaJIT only; keep Lua code LuaJIT-friendly and performance-oriented.
* Update types/carimbo.lua whenever C++ changes the Lua API.
* Use the project's pcall() wrapper for every Lua function call, never lua_pcall() or lua_call(). Check every call status and propagate all Lua errors to the catch block in src/application.cpp.
* Create Lua tables in C++ with lua_createtable, passing known array and hash sizes or zero for unknown sizes.
* Intern frequently pushed LuaJIT strings, but not one-off strings.
* Use assert and [[assume(...)]] only when necessary to protect memory, resource lifetime, or data integrity—not for routine checks of trusted inputs. Every assert must include a short, clear English error message.
* Fuzz all newly written code.
* Benchmark every performance-related change. Measure time, memory allocations, and copies before and after. Report the comparison and reject non-marginal regressions in each metric.
* Use a high-precision clock for benchmarks, never FPS. Measure release builds with NOVSYNC=1 and fullscreen enabled; never use WINDOWED=1.
* Run all benchmarks, profiling, sanitizers, fuzzing, leak detection, allocation tracking, and other instrumentation on macOS 27 Apple Silicon with Xcode Instruments and Apple Clang.
* Build and run macOS Apple Silicon binaries natively with Apple Clang.
* Use only the `Sandbox` CrossOver bottle to build Windows AMD64 binaries from source and run them. If `Sandbox` does not exist, create it and install the required build and runtime tools. Never create or use other bottles.
* Use modern, idiomatic C++23, preferring auto, constexpr, const, and final where appropriate.
* Minimize allocations, copies, and runtime overhead. Prefer O(1), SIMD-friendly, branchless, and cache-friendly implementations when practical.
* Use SIMDe for all explicit SIMD operations.
* Review struct and class member order for efficient memory layout and cache use.
* Use the project precompiled header. Keep all #include directives in common.hpp, except SDL3/SDL_main.h in main.cpp and single-header implementation includes in miniaudio.cpp and stb.cpp.
* Group includes by category and sort alphabetically within each group.
* Create helpers and abstractions only when they provide a clear benefit. Reuse alone is not a reason to create a helper. Helpers such as `checkerror` that only check a status and throw an error are forbidden. Repetition is acceptable and preferred when it keeps the code clear and direct.
* Prefer clear single-word variable names and unambiguous local abbreviations.
* Add a blank line before return only when another statement precedes it in the same block; never when return is the first statement in a block or file.
