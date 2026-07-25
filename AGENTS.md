# Rules

* Always assume happy path.
* Always make small, focused changes. Never refactor unrelated code or
  introduce unnecessarily complex solutions. **Simple and efficient always
  wins**. It's also acceptable to assume the happy path, since the input is
  provided by the author.
* Do not add any `#include` directives. We use PCH, and only `common.hpp` may
  contain includes, and only when absolutely necessary. The sole exception is
  `main.cpp`, which must include `SDL3/SDL_main.h` for the Win32 GUI entry point.
* Do not commit any changes.
* Do not create branches.
* Names for things (variables, classes, etc.) should be a single word whenever
  possible. If a single word isn't practical, use at most two or three words
  (`snake_case`), but always prioritize a single-word name.
* Local variables must always use one word or one acronym. Multiword local
  variable names are forbidden.
* Use or create a `struct` only when related data or behavior genuinely needs
  grouping; otherwise use direct declarations. Group related static variables
  only when necessary, using a non-instantiated `final` struct with one-word
  names for the struct and its members. Keep it in the narrowest applicable
  `.cpp`; place it in a header only when multiple translation units use it.
  Never put project-specific constant groups in `common.hpp`. Keep constants
  in the narrowest scope and use direct one-word names (`frequency`, not
  `sound::frequency`) whenever disambiguation is unnecessary. Do not attach
  constants to a type solely to qualify them. Keep a self-explanatory one-use
  literal directly at its call site and name constants when they are reused.
  A non-obvious semantic value must be named even when used once. The simplest
  direct declaration wins.
* Abbreviations and acronyms are discouraged outside local variables. Use them
  only when they are widely recognized and unambiguous.
* Use smart pointers only when they materially clarify ownership or prevent
  leaks and memory corruption without avoidable overhead. Use custom deleters
  when they add that value. Do not use a smart pointer when it adds overhead
  without a concrete return; raw non-owning pointers and directly managed C
  handles are valid when ownership and lifetime are clear and proven. Memory
  leaks and memory corruption are unacceptable.
* Prefer `std::make_unique_for_overwrite`.
* Whenever possible, implementations should have zero overhead: zero
  unnecessary copies, zero unnecessary allocations, and no avoidable runtime
  cost.
* Whenever possible, strive for O(1) time and space complexity, or the closest
  practical level of efficiency for the problem.
* Always avoid memory allocation.
* Always use language features such as `auto`, `final`, `const`, and `constexpr`
  whenever appropriate.
* There is no need for mutexes or `thread_local`. We do not use threads.
* Use modern C++, targeting C++23 whenever possible.
* Always benchmark performance before and after your changes to avoid
  regressions. Never compare performance in Debug mode. Always benchmark using
  Release builds only.
* Code must be portable across ARM64 and x86-64, as well as Windows and macOS.
  Most best-practice compiler warnings and checks should be enabled during
  development.
* Any changes to the Lua API exposed by C++ must be documented in
  `types/game.lua`.
* Use unit tests only to validate the correctness and efficiency of the code.
  Always measure performance, memory usage, and allocations, aiming for the
  fewest allocations, the lowest memory usage, and the highest possible
  performance and efficiency. For most changes, a smoke test is also required.
  Test everything thoroughly, then remove the tests afterward. Neither unit
  tests nor smoke tests should be committed or versioned.
* No guesswork. Every claim must be backed by evidence, either through empirical
  validation or reliable sources found online.
* No magic numbers. Keep self-explanatory one-use literals at clear call sites;
  name every non-obvious semantic value, even when used once.
* Don't worry about our custom formats. There's no need to maintain backward
  compatibility. Do the best for it.
* No inconsistencies of any kind are tolerated.
* Make use of `[[assume ..]];`. Assume happy path.
* Always use instrumentation.
* Always test with all available sanitizers. When appropriate, also perform fuzz testing.
* Only little-endian code.
* Always write SIMD-friendly and cache-friendly code.
