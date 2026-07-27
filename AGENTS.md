# Agent Rules

## Core

### Scope

- Assume valid, author-provided input; **optimize for the happy path**.
- Make only small, focused changes. **Never refactor unrelated code.**
- Prefer the simplest consistent solution.
- **Do not create branches or commits.**
- Custom formats need no backward compatibility.

### Platforms

- Support only little-endian systems.
- Support only Support Apple Silicon on macOS and AMD64 on Windows.

## Languages

### C++

#### Standard and Files

- Target C++23; use modern features such as `auto`, `final`, `const`, and `constexpr` when appropriate.
- **Add includes only to `common.hpp` and only when necessary.**
- For Win32, `main.cpp` must include `SDL3/SDL_main.h`.

#### Naming

- Prefer one-word names.
- Local variables must use one word or acronym. Use short `snake_case` only when one word is impractical.
- Outside local variables, use abbreviations only when widely recognized and unambiguous.

#### Types and State

- Use structs only to group related data or behavior. Keep them in the narrowest `.cpp`; use a header only when shared.
- Group related static variables only when necessary. Use a non-instantiated `final` struct with a one-word name.
- Keep constants in the narrowest scope.
- Never place project-specific constant groups in `common.hpp` or create types only to qualify constants.
- Keep clear, one-use literals inline. Name reused values and every non-obvious semantic value.

#### Ownership and Safety

- Use smart pointers only when they improve ownership or safety without needless overhead. Prefer `std::make_unique_for_overwrite`.
- Raw non-owning pointers and managed C handles are valid when ownership and lifetime are clear.
- **Never allow leaks or memory corruption.**

#### Performance and Concurrency

- The project is single-threaded. **Do not use mutexes or `thread_local`.**
- Avoid allocations, copies, and runtime overhead.
- Favor O(1), SIMD-friendly, and cache-friendly implementations.
- Use `[[assume(...)]]`, `likely`, and `unlikely` when they improve the happy path.

#### Lua API

- Document every C++ change to the Lua API in `types/game.lua`.

### Lua

- **LuaJIT only**
- Lua code **must be LuaJIT-friendly and performance-oriented**.

## Verification

### Evidence and Benchmarks

- Instrument changes; support every claim with measurements or reliable sources.
- **Benchmark before and after every change in Release mode.** Never compare Debug benchmarks.
- Measure runtime, memory use, and allocations.
- Inspect generated assembly; do not rely on benchmarks alone.

### Quality Checks

- Enable strong compiler warnings and checks during development.
- Test with every available sanitizer; fuzz when appropriate.
- Use unit and smoke tests to validate correctness and efficiency.
- Remove temporary tests before finishing; **never version them**.

## Tools

- Use Docker on Linux for better instrumentation.
- Use Wine when Windows validation is required.
