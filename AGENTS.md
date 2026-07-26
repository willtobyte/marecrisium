# Rules

## Scope

- Assume valid, author-provided input and optimize for the happy path.
- Make small, focused changes. Never refactor unrelated code.
- Prefer the simplest consistent solution.
- Do not create branches or commits.
- Custom formats do not require backward compatibility.
- Support little-endian systems only.

## C++

- Target C++23 and use modern features such as `auto`, `final`, `const`, and `constexpr` when appropriate.
- Do not add includes outside `common.hpp`; add them there only when necessary. `main.cpp` must include `SDL3/SDL_main.h` for Win32.
- Support ARM64 and x86-64 on Windows and macOS.
- Do not use mutexes or `thread_local`; the project is single-threaded.
- Prefer one-word names. Local variables must use one word or acronym. Use short `snake_case` names only when one word is impractical.
- Use abbreviations outside local variables only when widely recognized and unambiguous.
- Use structs only to group related data or behavior. Keep them in the narrowest `.cpp`; use a header only when shared.
- Group related static variables only when needed, using a non-instantiated `final` struct with one-word names.
- Keep constants in the narrowest scope. Never place project-specific constant groups in `common.hpp` or create types only to qualify constants.
- Keep clear one-use literals inline. Name reused values and every non-obvious semantic value.
- Use smart pointers only when they improve ownership or safety without needless overhead. Prefer `std::make_unique_for_overwrite`.
- Raw non-owning pointers and managed C handles are valid when ownership and lifetime are clear. Never allow leaks or memory corruption.
- Avoid allocations, copies, and runtime overhead. Favor O(1), SIMD-friendly, and cache-friendly implementations.
- Use `[[assume(...)]]`, `likely`, and `unlikely` when they improve the happy path.
- Document every C++ change to the Lua API in `types/game.lua`.

## Verification

- Instrument changes and support every claim with measurements or reliable sources.
- Benchmark before and after changes in Release mode only. Never use Debug benchmarks for comparison.
- Measure runtime, memory use, and allocations. Inspect generated assembly; do not rely on benchmarks alone.
- Enable strong compiler warnings and checks during development.
- Test with every available sanitizer. Fuzz when appropriate.
- Use unit and smoke tests to validate correctness and efficiency. Remove temporary tests before finishing; never version them.
