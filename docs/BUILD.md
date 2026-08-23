# Building

Install the repository-declared tools using [mise](https://mise.jdx.dev), then set up the Conan profile and dependencies:

```shell
mise install
conan profile detect --force
make conan
```

Build and run:

```shell
make run # for debug builds
# or
make conan build buildtype=Release && ./build/carimbo # for release
```

Generate an object from its TexturePacker JSON file:

```shell
uv run assets/tools/convert.py assets/objects/<name>/<name>.json
```
