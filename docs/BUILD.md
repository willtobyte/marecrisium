# Building

Install the repository-declared tools, then set up the Conan profile and dependencies:

```shell
mise install
conan profile detect --force
make conan
```

Build and run:

```shell
make run # debug
# or
make conan build buildtype=Release && ./build/carimbo
```
