# Mare Crisium

[![macOS](https://github.com/willtobyte/frenesi/actions/workflows/macos.yaml/badge.svg)](https://github.com/willtobyte/frenesi/actions/workflows/macos.yaml)
[![Windows](https://github.com/willtobyte/frenesi/actions/workflows/windows.yaml/badge.svg)](https://github.com/willtobyte/frenesi/actions/workflows/windows.yaml)

> - From where comes all this disbelief? It looks like a revolt... Against who?
> - You have too much to learn, Antônio. I can't have disbelief if I never had belief...
> To believe in what? In a symbol? In an absent force created by ignorance?
> Yes, I'm a rebel... Against the fools like you!

## Engine

Carimbo is an engine written in modern C++, using Box2D, EnTT,
libspng, LuaJIT, miniaudio, opusfile, PhysFS, SDL, SQLite, and yyjson.

The engine exposes a minimal interface to games;
all heavy lifting is done on the C++ side.

## Game

The game is written in pure Lua with JIT. See the cartridge directory.
Types, annotations, and everything related to the exposed Lua API
are documented in `types/carimbo.lua`.

## Building

See [docs/BUILDING.md](docs/BUILDING.md) for build and run instructions.

## Modifications

See [docs/MODIFICATIONS.md](docs/MODIFICATIONS.md) for modifications support.

## Design

See [docs/GDD.md](docs/gdd.md) for the game design document.
