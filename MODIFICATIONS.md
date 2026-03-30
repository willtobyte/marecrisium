# Modifications

The engine supports a file-overlay system that allows modifications
to override original game content without altering the base cartridge.

## How It Works

At startup, the engine mounts the game content (the `cartridge/`
directory or `cartridge.rom` archive) and then attempts to mount
a `modifications.zip` archive. Files inside `modifications.zip`
take **higher precedence** than files in the cartridge. When the
engine reads a file, it searches `modifications.zip` first; if
the file exists there, it is used instead of the original.

If `modifications.zip` does not exist, the engine runs normally
with just the base cartridge content.

## Usage

Create a `modifications.zip` archive alongside the game executable
(or in the working directory) containing files using the same
directory structure as the cartridge. For example, to replace
a sprite:

```text
modifications.zip
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
