# Building

Install the repository-declared tools using [mise](https://mise.jdx.dev), then set up the Conan profile and dependencies:

```shell
mise install
conan profile detect --force
make conan
```

Build and run:

```shell
make conan build MODE={Release|Debug} && ./build/carimbo
```

Generate all fonts and objects:

```shell
uv run assets/tools/run.py
```

See [Objects](OBJECTS.md) for the object source format.
