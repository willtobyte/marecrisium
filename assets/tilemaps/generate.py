#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.13"
# dependencies = ["numpy", "pyoxipng"]
# ///
"""Compile a Tiled .tmx tilemap into the single runtime .bmap binary the C++
engine consumes, plus copy/optimise referenced tileset PNGs.

Source of truth: ``assets/tilemaps/<name>.tmx``.

Outputs:
  cartridge/tilemaps/<name>.bmap                single binary tilemap
                                                (display + collision)
  cartridge/blobs/tilemaps/<name>/<tileset>.png copied + oxipng-optimised
                                                tileset images

Usage::

    ./generate.py <name>

The .tmx must have orthogonal orientation, square tiles, and three CSV
layers named ``background``, ``foreground`` and ``collision``.

.bmap layout (little-endian, tightly packed):
  Header (20 bytes):
    uint32  width
    uint32  height
    float32 size
    uint64  source_hash    (BLAKE2b-64 of the .tmx)
  Payload (N = width * height):
    uint32  background[N]
    uint32  foreground[N]
    uint8   collision[N]
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import struct
import sys
import xml.etree.ElementTree as ET

import numpy as np
import oxipng


def csv(text: str | None, expected: int, label: str) -> list[int]:
    tokens = re.findall(r"-?\d+", text or "")
    if len(tokens) != expected:
        raise SystemExit(
            f"layer '{label}' has {len(tokens)} entries, expected {expected}"
        )
    return [int(token) for token in tokens]


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.normpath(os.path.join(here, "..", ".."))

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name", help="Tilemap name (without extension).")
    args = parser.parse_args()
    name = args.name

    source = os.path.join(here, f"{name}.tmx")
    if not os.path.isfile(source):
        print(f"Not found: {source}", file=sys.stderr)
        return 1

    document = ET.parse(source).getroot()
    if document.tag != "map":
        raise SystemExit(f"{source}: not a Tiled .tmx (root <{document.tag}>)")
    if document.get("orientation") != "orthogonal":
        raise SystemExit(f"{source}: only orthogonal orientation supported")

    width = int(document.get("width", "0"))
    height = int(document.get("height", "0"))
    tilewidth = int(document.get("tilewidth", "0"))
    tileheight = int(document.get("tileheight", "0"))
    if width <= 0 or height <= 0:
        raise SystemExit(f"{source}: invalid dimensions {width}x{height}")
    if tilewidth != tileheight or tilewidth <= 0:
        raise SystemExit(f"{source}: only square non-zero tiles supported")
    if width * height > 4096 * 4096:
        raise SystemExit(f"map {width}x{height} exceeds 4096x4096 budget")

    expected = width * height
    layers: dict[str, list[int]] = {}
    for layer in document.findall("layer"):
        label = layer.get("name", "")
        if label not in {"background", "foreground", "collision"}:
            continue
        data = layer.find("data")
        if data is None or data.get("encoding") != "csv":
            raise SystemExit(
                f"{source}: layer '{label}' must be CSV-encoded "
                "(Tiled Map -> Map Properties -> Tile Layer Format -> CSV)"
            )
        layers[label] = csv(data.text, expected, label)

    if "collision" not in layers:
        raise SystemExit(f"{source}: missing required 'collision' layer")
    background = layers.get("background") or [0] * expected
    foreground = layers.get("foreground") or [0] * expected
    collision = layers["collision"]

    walkable = sum(1 for value in collision if value == 0)
    print(f"[{name}] {width}x{height} cells, walkable={walkable}/{expected}")

    with open(source, "rb") as handle:
        digest = hashlib.blake2b(handle.read(), digest_size=8).digest()

    runtime = os.path.join(root, "cartridge", "tilemaps")
    os.makedirs(runtime, exist_ok=True)
    output = os.path.join(runtime, f"{name}.bmap")

    with open(output, "wb") as handle:
        handle.write(
            struct.pack(
                "<IIfQ",
                np.uint32(width),
                np.uint32(height),
                float(tilewidth),
                int.from_bytes(digest[:8], "little"),
            )
        )
        np.asarray(background, dtype="<u4").tofile(handle)
        np.asarray(foreground, dtype="<u4").tofile(handle)
        np.asarray(collision, dtype="u1").tofile(handle)

    total = os.path.getsize(output)
    if total != 20 + 9 * expected:
        raise SystemExit(
            f"{output}: produced {total} bytes, expected {20 + 9 * expected}"
        )
    print(f"Wrote {output} ({total} bytes)")

    base = os.path.dirname(os.path.abspath(source))
    blob = os.path.join(root, "cartridge", "blobs", "tilemaps", name)
    os.makedirs(blob, exist_ok=True)
    for tileset in document.findall("tileset"):
        label = tileset.get("name", "")
        image = tileset.find("image")
        if not label or image is None:
            continue
        relative = image.get("source", "")
        if not relative:
            continue
        picture = os.path.normpath(os.path.join(base, relative))
        if not os.path.isfile(picture):
            print(f"Warning: tileset image missing: {picture} (skipping)")
            continue
        target = os.path.join(blob, f"{label}.png")
        shutil.copy2(picture, target)
        oxipng.optimize(
            target,
            level=6,
            filter=[oxipng.RowFilter.Brute],
            strip=oxipng.StripChunks.safe(),
            interlace=oxipng.Interlacing.Off,
        )

        print(f"Wrote {target} (optimised)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
