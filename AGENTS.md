# Project instructions

## General

- Support little-endian systems only.
- Assume the happy path. Treat inputs as safe and trusted.
- Always write code that is simple for humans to understand.
- Never create Git branches or commits.

## C++ and SIMD

- Use modern, idiomatic C++23.
- Prefer `auto`, `constexpr`, `const`, and `final` when appropriate.
- Create functions, methods, and abstractions only when they have more than one use.
- Use single-word variable names when they are clear.
- Prefer clear abbreviations for local variables when they remain unambiguous.
- Add a blank line before `return` when another statement precedes it in the same block.
- Do not add a blank line when `return` is the first statement in a block or file.
- Minimize allocations, copies, and runtime overhead.
- Prefer O(1), SIMD-friendly, branchless, and cache-friendly implementations when practical.
- Use [SIMDe](https://github.com/simd-everywhere/simde) for all explicit SIMD operations.
- Keep the code compatible with Apple Silicon from M1 onward, ARM64, and Intel CPUs released within the last 10 years.
- Review struct and class member order for memory layout and cache efficiency.
- Place all `#include` directives in `common.hpp`.
- Allow `#include <SDL3/SDL_main.h>` only in `main.cpp`.
- Allow single-header implementation includes only in `miniaudio.cpp` and `stb.cpp`.
- Group includes by category and sort them alphabetically.
- Use the project precompiled header.

## Lua and LuaJIT

- Support LuaJIT only.
- Keep Lua code LuaJIT-friendly and performance-oriented.
- Update `types/carimbo.lua` with every C++ change to the Lua API.
- Use `lua_createtable` for every explicit Lua table created in C++.
- Pass known array and hash sizes to `lua_createtable`.
- Pass zero for unknown array or hash sizes.
- Use the project `pcall()` wrapper for all Lua function calls.
- Never call `lua_pcall()` or `lua_call()` directly.
- Check every Lua call status.
- Propagate every Lua error to the `catch` block in `src/application.cpp`.
- Intern LuaJIT strings that are pushed frequently, such as strings used in every loop iteration or mouse-move event.
- Do not intern one-off strings.
- Use the object's `patch.lua.j2` for object changes.
- Do not edit generated files in `cartridge/objects/` directly.

## Assertions and validation

- Use `assert` and `[[assume(...)]]` only when necessary to protect memory, resource lifetime, or data integrity.
- Do not use them on the happy path or for routine checks of trusted inputs.
- Include a short, clear English error message in every `assert`.
- Validate all newly created code with fuzzing.
- Keep instrumentation code, tests, benchmarks, profiling tools, and similar artifacts outside the project directory.
- Run benchmarks, profiling, sanitizers, fuzzing, memory-leak detection, allocation tracking, and other instrumentation on macOS 27 with Xcode Instruments and Apple Clang.

## Performance

- Benchmark every performance-related change.
- Measure before and after each meaningful performance change.
- Reject performance regressions unless they are marginal.
- Report the measurements and comparison.
- Use a high-precision clock for benchmarks.
- Never use FPS as a benchmark metric.
- Run release performance measurements with `NOVSYNC=1` and fullscreen enabled.
- Do not use `WINDOWED=1`.

## Docker

- Use the remote Docker host at `92.112.178.107` through SSH and `DOCKER_HOST` for all Linux AMD64 and Windows AMD64 tests, with Windows tests running under Wine64 in Linux containers.
- Use local Docker through OrbStack only when Linux ARM64 is required or the remote host is unavailable.
- Before every Docker command, remove all unused Docker data.
