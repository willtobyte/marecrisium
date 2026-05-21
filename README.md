# Mare Crisium

[![macOS](https://github.com/willtobyte/frenesi/actions/workflows/macos.yml/badge.svg)](https://github.com/willtobyte/frenesi/actions/workflows/macos.yml)
[![Windows](https://github.com/willtobyte/frenesi/actions/workflows/windows.yml/badge.svg)](https://github.com/willtobyte/frenesi/actions/workflows/windows.yml)

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
