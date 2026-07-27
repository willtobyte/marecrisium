# Rules

## General

- Assume valid, author-provided input. Optimize for the happy path.
- Keep changes small, focused, and minimal. Never refactor unrelated code.
- Prefer the simplest correct solution.
- Do not create branches or commits.
- Backward compatibility is not required for project-specific formats.

## Platforms

- Support only little-endian systems.
- Support Apple Silicon on macOS and AMD64 on Windows.
- Always use Linux Docker with an AMD64 userspace and Wine for Windows builds and testing. **Never use Wine on macOS!**.

## C++

- Target C++23.
- Always prefer `constexpr` whenever applicable.
- Prefer modern C++ (`auto`, `constexpr`, `const`, `final`) when appropriate.
- Add headers only to `common.hpp`, and only when necessary.
- On Win32, `main.cpp` must include `SDL3/SDL_main.h`.
- Always use `noexcept` where necessary. Do not swallow important exceptions. Exceptions such as allocation failures may be ignored.

### Style

- Prefer short, descriptive names.
- Local variables should be a single word whenever practical.
- Use a single word or abbreviation for local-scope variable names.

### Design

- Keep constants, types, and state in the narrowest possible scope.
- Use `struct` only to group closely related data or behavior.
- Do not place project-specific constants in `common.hpp`.

### Performance

- The project is strictly single-threaded. Never use mutexes or `thread_local`.
- Minimize allocations, copies, and runtime overhead.
- Prefer O(1), SIMD-friendly, and cache-friendly implementations.
- Use `[[assume]]`, `likely`, and `unlikely` when they measurably improve the hot path.

### Memory

- Prefer `std::make_unique_for_overwrite` when appropriate.
- Raw non-owning pointers are acceptable when ownership is explicit.
- Never introduce memory leaks or undefined behavior.

### Lua API

- Every C++ change to the Lua API must also update `types/game.lua`.

## Lua

- LuaJIT only.
- Keep Lua code LuaJIT-friendly and performance-oriented.

## Validation

- Measure every change in **Release** mode before and after.
- Always collect runtime, memory usage, and allocation metrics.
- Inspect generated assembly; benchmarks alone are insufficient.
- Enable compiler warnings, sanitizers, and fuzzing whenever applicable.
- Validate with unit and smoke tests, then remove temporary tests before finishing.
