# Modifications

The engine supports a file-overlay system that allows modifications
to override original game content without altering the base cartridge.

## How It Works

At startup, the engine mounts the game content (the `cartridge/`
directory or `cartridge.rom` archive) and then attempts to mount
modifications from one of two sources:

1. A `modifications.zip` archive
2. A `modifications/` directory

If both exist, `modifications.zip` is used. Files inside the
modifications layer take **higher precedence** than files in
the cartridge. When the engine reads a file, it searches the
modifications layer first; if the file exists there, it is used
instead of the original.

If neither `modifications.zip` nor `modifications/` exist, the
engine runs normally with just the base cartridge content.

## Usage

Place a `modifications.zip` archive or a `modifications/`
directory alongside the game executable (or in the working
directory) with files using the same directory structure as the
cartridge. For example, to replace a sprite:

```text
modifications.zip          (or modifications/)
  blobs/
    player.png
```

This `player.png` will be loaded instead of the one from the
cartridge.

## What Can Be Modified

Any file that the game reads through the virtual filesystem can
be overridden, including:

- Sprites and images
- Lua scripts
- Tilemaps
- Sounds and music
