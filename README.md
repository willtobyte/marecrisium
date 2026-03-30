# Mare Crisium

[![macOS](https://github.com/willtobyte/frenesi/actions/workflows/macos.yaml/badge.svg)](https://github.com/willtobyte/frenesi/actions/workflows/macos.yaml)
[![Windows](https://github.com/willtobyte/frenesi/actions/workflows/windows.yaml/badge.svg)](https://github.com/willtobyte/frenesi/actions/workflows/windows.yaml)

> - From where comes all this disbelief? It looks like a revolt... Against who?
> - You have too much to learn, Antônio. I can't have disbelief if I never had belief...
> To believe in what? In a symbol? In an absent force created by ignorance?
> Yes, I'm a rebel... Against the fools like you!

## Game & Engine

Carimbo is an engine written in modern C++ (C++23), using Box2D, EnTT,
libspng, LuaJIT, miniaudio, opusfile, PhysFS, SDL, SQLite, and yyjson.

The engine exposes a minimal interface to games;
all heavy lifting is done on the C++ side.

The game is written in pure Lua with JIT.

### Run

Install Conan (choose one method)

```shell
mise use -g conan # recommended
# brew install conan
# pip install conan
```

Initialize Conan profile (only once)

```shell
conan profile detect --force
```

Install dependencies, (also only once)

```shell
make conan
```

Build and run (debug)

```shell
make run
```

Build and run (release)

```shell
make conan build buildtype=Release && CARTRIDGE=cartridge ./build/carimbo
```
