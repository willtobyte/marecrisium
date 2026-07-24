# Rules

* Always assume happy path.
* Always make small, focused changes. Never refactor unrelated code or introduce unnecessarily complex solutions. **Simple and efficient always wins**. It's also acceptable to assume the happy path, since the input is provided by the author.
* Do not add any `#include` directives. We use PCH, and only `common.h` may contain includes, and only when absolutely necessary.
* Do not commit any changes.
* Do not create branches.
* Names for things (variables, classes, etc.) should be a single word whenever possible. For local or small-scope variables, prefer short abbreviations. If a single word isn't practical, use at most two or three word (snake_case), but always prioritize a single-word name.
* Abbreviations and acronyms are discouraged. Use them only when they are widely recognized and unambiguous.
* Always prefer to use smart pointers when appropriate, and use custom deleters when they make sense. Raw pointers should be reserved for exceptional cases where they provide a clear, proven benefit in readability or performance. Memory leaks and memory corruption are unacceptable.
* Prefer std::make_unique_for_overwrite.
* Whenever possible, implementations should have zero overhead: zero unnecessary copies, zero unnecessary allocations, and no avoidable runtime cost.
* Whenever possible, strive for O(1) time and space complexity, or the closest practical level of efficiency for the problem.
* Always avoid memory allocation.
* Always use language features such as `auto`, `final`, `const`, and `constexpr` whenever appropriate.
* There is no need for mutexes or `thread_local`. We do not use threads.
* Use modern C++, targeting C++26 whenever possible.
* Always benchmark performance before and after your changes to avoid regressions. Never compare performance in Debug mode. Always benchmark using Release builds only.
* Code must be portable across ARM64 and x86-64, as well as Windows and macOS. Most best-practice compiler warnings and checks should be enabled during development.
* Any changes to the Lua API exposed by C++ must be documented in `types/game.lua`.
* Use unit tests only to validate the correctness and efficiency of the code. Always measure performance, memory usage, and allocations, aiming for the fewest allocations, the lowest memory usage, and the highest possible performance and efficiency. For most changes, a smoke test is also required. Test everything thoroughly, then remove the tests afterward. Neither unit tests nor smoke tests should be committed or versioned.
* No guesswork. Every claim must be backed by evidence, either through empirical validation or reliable sources found online.
* No magic numbers. Create a constant or add a comment.
* Don't worry about our custom formats. There's no need to maintain backward compatibility. Do the best for it.
* No inconsistencies of any kind are tolerated.
* Make use of `[[assume ..]];`. Assume happy path.
