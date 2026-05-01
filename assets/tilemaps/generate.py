#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.13"
# dependencies = ["numpy", "pyoxipng"]
# ///
"""Compile a Tiled .tmx tilemap into the single runtime .map binary the C++
engine consumes, plus copy/optimise referenced tileset PNGs.

Source of truth: ``assets/tilemaps/<name>.tmx``.

Outputs:
  cartridge/tilemaps/<name>.map                 single binary tilemap
                                                (display + JPS+ + CC)
  cartridge/blobs/tilemaps/<name>/<tileset>.png copied + oxipng-optimised
                                                tileset images

Usage::

    ./generate.py <name>

The .tmx must have orthogonal orientation, square tiles, and three CSV
layers named ``background``, ``foreground`` and ``collision``. Optional
map property ``radius_tiles`` (int) inflates the collision grid by that
many tiles before the path data is computed, baking a nominal agent
body radius into the navigation grid.

.map layout (little-endian, tightly packed):
  Header (24 bytes):
    uint32  width
    uint32  height
    float32 size
    uint32  radius_tiles
    uint64  source_hash    (BLAKE2b-64 of the .tmx)
  Payload (N = width * height):
    uint32  background[N]
    uint32  foreground[N]
    uint8   collision[N]
    uint16  components[N]      (0xFFFF = blocked)
    int16   jump[8 * N]        (8 directions, JPS+ jump table)
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
from dataclasses import dataclass, field

import numpy as np
import oxipng

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SOURCE_DIR = HERE
RUNTIME_DIR = os.path.join(ROOT, "cartridge", "tilemaps")
BLOBS_DIR = os.path.join(ROOT, "cartridge", "blobs", "tilemaps")

# Direction encoding mirrors tilemap.cpp DC[]/DR[].
#   0:E  1:W  2:S  3:N  4:SE  5:NE  6:SW  7:NW
DC = (1, -1, 0, 0, 1, 1, -1, -1)
DR = (0, 0, 1, -1, 1, -1, 1, -1)

CARDINALS = (0, 1, 2, 3)
DIAGONALS = (4, 5, 6, 7)

# For each diagonal, the two cardinal sub-components (indices into DR/DC).
# SE(4) -> {S,E},  NE(5) -> {N,E},  SW(6) -> {S,W},  NW(7) -> {N,W}.
DIAG_SUBS = {
    4: (2, 0),
    5: (3, 0),
    6: (2, 1),
    7: (3, 1),
}


@dataclass
class Tileset:
    name: str
    image: str  # absolute path to source PNG


@dataclass
class MapData:
    name: str
    width: int
    height: int
    tile_size: int
    background: list[int]
    foreground: list[int]
    collision: list[int]
    radius_tiles: int = 0
    tilesets: list[Tileset] = field(default_factory=list)


def _parse_csv_layer(data_text: str, expected: int) -> list[int]:
    tokens = re.findall(r"-?\d+", data_text)
    if len(tokens) != expected:
        raise SystemExit(f"layer data has {len(tokens)} entries, expected {expected}")
    return [int(token) for token in tokens]


def parse_tmx(path: str, name: str) -> MapData:
    tree = ET.parse(path)
    root = tree.getroot()
    if root.tag != "map":
        raise SystemExit(f"{path}: not a Tiled .tmx (root <{root.tag}>)")

    if root.get("orientation") != "orthogonal":
        raise SystemExit(f"{path}: only orthogonal orientation supported")

    width = int(root.get("width", "0"))
    height = int(root.get("height", "0"))
    tw = int(root.get("tilewidth", "0"))
    th = int(root.get("tileheight", "0"))
    if width <= 0 or height <= 0:
        raise SystemExit(f"{path}: invalid dimensions {width}x{height}")
    if tw != th or tw <= 0:
        raise SystemExit(f"{path}: only square non-zero tiles supported")

    radius_tiles = 0
    properties = root.find("properties")
    if properties is not None:
        for prop in properties.findall("property"):
            if prop.get("name") == "radius_tiles":
                radius_tiles = int(prop.get("value", "0"))

    base = os.path.dirname(os.path.abspath(path))
    tilesets: list[Tileset] = []
    for ts in root.findall("tileset"):
        ts_name = ts.get("name", "")
        if not ts_name:
            continue
        image = ts.find("image")
        if image is None:
            continue
        source = image.get("source", "")
        if not source:
            continue
        tilesets.append(
            Tileset(name=ts_name, image=os.path.normpath(os.path.join(base, source)))
        )

    expected = width * height
    layers: dict[str, list[int]] = {}
    for layer in root.findall("layer"):
        layer_name = layer.get("name", "")
        if layer_name not in {"background", "foreground", "collision"}:
            continue
        data = layer.find("data")
        if data is None or data.get("encoding") != "csv":
            raise SystemExit(
                f"{path}: layer '{layer_name}' must be CSV-encoded (Tiled "
                "Map -> Map Properties -> Tile Layer Format -> CSV)"
            )
        layers[layer_name] = _parse_csv_layer(data.text or "", expected)

    if "collision" not in layers:
        raise SystemExit(f"{path}: missing required 'collision' layer")
    background = layers.get("background") or [0] * expected
    foreground = layers.get("foreground") or [0] * expected

    return MapData(
        name=name,
        width=width,
        height=height,
        tile_size=tw,
        background=background,
        foreground=foreground,
        collision=layers["collision"],
        radius_tiles=radius_tiles,
        tilesets=tilesets,
    )


def chebyshev_distance(blocked: np.ndarray) -> np.ndarray:
    """Per-cell Chebyshev distance to the nearest blocked cell. Blocked cells
    get 0; walkable cells get the BFS distance (8-connected)."""
    height, width = blocked.shape
    if not blocked.any():
        return np.full((height, width), np.iinfo(np.int16).max, dtype=np.int16)
    dist = np.where(blocked, np.int16(0), np.int16(np.iinfo(np.int16).max))

    frontier = list(zip(*np.where(blocked)))
    head = 0
    while head < len(frontier):
        r, c = frontier[head]
        head += 1
        d = dist[r, c]
        nd = d + 1
        for k in range(8):
            nr = r + DR[k]
            nc = c + DC[k]
            if 0 <= nr < height and 0 <= nc < width and dist[nr, nc] > nd:
                dist[nr, nc] = nd
                frontier.append((nr, nc))
    return dist


def inflate(blocked: np.ndarray, radius_tiles: int) -> np.ndarray:
    if radius_tiles <= 0:
        return blocked.copy()
    dist = chebyshev_distance(blocked)
    return dist < (radius_tiles + 1)


def connected_components(blocked: np.ndarray) -> np.ndarray:
    """Label walkable cells with CC ids (uint16); blocked cells get 0xFFFF.
    8-connected with no diagonal corner-cutting (matches the runtime)."""
    height, width = blocked.shape
    labels = np.full((height, width), 0xFFFF, dtype=np.uint16)
    next_id = 1

    for sr in range(height):
        for sc in range(width):
            if blocked[sr, sc] or labels[sr, sc] != 0xFFFF:
                continue
            label = np.uint16(next_id)
            stack = [(sr, sc)]
            labels[sr, sc] = label
            while stack:
                r, c = stack.pop()
                for k in range(8):
                    dr, dc = DR[k], DC[k]
                    nr, nc = r + dr, c + dc
                    if not (0 <= nr < height and 0 <= nc < width):
                        continue
                    if blocked[nr, nc] or labels[nr, nc] != 0xFFFF:
                        continue
                    if dr != 0 and dc != 0:
                        if blocked[r, nc] or blocked[nr, c]:
                            continue
                    labels[nr, nc] = label
                    stack.append((nr, nc))
            next_id += 1
            if next_id >= 0xFFFE:
                raise SystemExit("too many connected components (>=65534)")
    return labels


def _diagonal_safe(blocked: np.ndarray, r: int, c: int, dr: int, dc: int) -> bool:
    height, width = blocked.shape
    nr, nc = r + dr, c + dc
    if not (0 <= nr < height and 0 <= nc < width):
        return False
    if blocked[nr, nc]:
        return False
    if blocked[r, nc] or blocked[nr, c]:
        return False
    return True


def _is_cardinal_jump_point(blocked: np.ndarray, r: int, c: int, d: int) -> bool:
    """A walkable cell (r,c) reached by moving in cardinal direction d is a
    primary jump point if it has a forced neighbor on either perpendicular
    side. The parent is at (r - DR[d], c - DC[d])."""
    height, width = blocked.shape
    pr, pc = r - DR[d], c - DC[d]
    if d in (0, 1):
        # Cardinal East/West: perpendicular axis is row.
        for dr in (-1, 1):
            par_perp_r, par_perp_c = pr + dr, pc
            forward_r, forward_c = r + dr, c + DC[d]
            if not (0 <= par_perp_r < height and 0 <= par_perp_c < width):
                continue
            if not (0 <= forward_r < height and 0 <= forward_c < width):
                continue
            if blocked[par_perp_r, par_perp_c] and not blocked[forward_r, forward_c]:
                return True
    else:
        # Cardinal North/South: perpendicular axis is column.
        for dc in (-1, 1):
            par_perp_r, par_perp_c = pr, pc + dc
            forward_r, forward_c = r + DR[d], c + dc
            if not (0 <= par_perp_r < height and 0 <= par_perp_c < width):
                continue
            if not (0 <= forward_r < height and 0 <= forward_c < width):
                continue
            if blocked[par_perp_r, par_perp_c] and not blocked[forward_r, forward_c]:
                return True
    return False


def _is_diagonal_jump_point(blocked: np.ndarray, r: int, c: int, d: int) -> bool:
    """Forced-neighbor check for diagonal direction d arriving at (r,c)."""
    height, width = blocked.shape
    dr, dc = DR[d], DC[d]
    # Check (r-dr, c) blocked while (r-dr, c+dc) walkable -> forced neighbor.
    a_r, a_c = r - dr, c
    b_r, b_c = r - dr, c + dc
    if (
        0 <= a_r < height
        and 0 <= a_c < width
        and 0 <= b_r < height
        and 0 <= b_c < width
        and blocked[a_r, a_c]
        and not blocked[b_r, b_c]
    ):
        return True
    # Check (r, c-dc) blocked while (r+dr, c-dc) walkable -> forced neighbor.
    e_r, e_c = r, c - dc
    f_r, f_c = r + dr, c - dc
    if (
        0 <= e_r < height
        and 0 <= e_c < width
        and 0 <= f_r < height
        and 0 <= f_c < width
        and blocked[e_r, e_c]
        and not blocked[f_r, f_c]
    ):
        return True
    return False


def _cardinal_iter(width: int, height: int, d: int):
    if d == 0:  # East -> right-to-left
        for r in range(height):
            for c in range(width - 1, -1, -1):
                yield r, c
    elif d == 1:  # West -> left-to-right
        for r in range(height):
            for c in range(width):
                yield r, c
    elif d == 2:  # South -> bottom-to-top
        for r in range(height - 1, -1, -1):
            for c in range(width):
                yield r, c
    else:  # North -> top-to-bottom
        for r in range(height):
            for c in range(width):
                yield r, c


def _diagonal_iter(width: int, height: int, d: int):
    dr, dc = DR[d], DC[d]
    rs = range(height - 1, -1, -1) if dr > 0 else range(height)
    cs = list(range(width - 1, -1, -1)) if dc > 0 else list(range(width))
    for r in rs:
        for c in cs:
            yield r, c


def compute_jump_tables(blocked: np.ndarray) -> np.ndarray:
    """Compute JPS+ jump distances per (direction, cell) on the inflated grid.

    Encoding (int16):
      > 0  : the next jump-point lies that many cells away in direction d.
      <= 0 : no jump-point ahead; |value| is the maximum number of walkable
             steps the agent can take before being blocked. Zero means the
             cell directly in direction d is blocked (or the cell itself is
             blocked).
    """
    height, width = blocked.shape
    n = height * width
    jump = np.zeros((8, n), dtype=np.int16)
    flat = blocked.reshape(-1)

    def idx(r: int, c: int) -> int:
        return r * width + c

    for d in CARDINALS:
        dr, dc = DR[d], DC[d]
        for r, c in _cardinal_iter(width, height, d):
            i = idx(r, c)
            if flat[i]:
                continue
            nr, nc = r + dr, c + dc
            if not (0 <= nr < height and 0 <= nc < width) or flat[idx(nr, nc)]:
                jump[d, i] = 0
                continue
            if _is_cardinal_jump_point(blocked, nr, nc, d):
                jump[d, i] = 1
                continue
            ahead = jump[d, idx(nr, nc)]
            jump[d, i] = ahead + 1 if ahead > 0 else ahead - 1

    for d in DIAGONALS:
        dr, dc = DR[d], DC[d]
        sub_v, sub_h = DIAG_SUBS[d]
        for r, c in _diagonal_iter(width, height, d):
            i = idx(r, c)
            if flat[i]:
                continue
            if not _diagonal_safe(blocked, r, c, dr, dc):
                jump[d, i] = 0
                continue
            ni = idx(r + dr, c + dc)
            if (
                _is_diagonal_jump_point(blocked, r + dr, c + dc, d)
                or jump[sub_v, ni] != 0
                or jump[sub_h, ni] != 0
            ):
                jump[d, i] = 1
                continue
            ahead = jump[d, ni]
            jump[d, i] = ahead + 1 if ahead > 0 else ahead - 1

    return jump


def emit_map(
    output: str,
    map_data: MapData,
    cc: np.ndarray,
    jump: np.ndarray,
    source_hash: bytes,
) -> None:
    width = map_data.width
    height = map_data.height
    n = width * height
    assert cc.shape == (height, width)
    assert jump.shape == (8, n)

    header = struct.pack(
        "<IIfIQ",
        np.uint32(width),
        np.uint32(height),
        float(map_data.tile_size),
        np.uint32(map_data.radius_tiles),
        int.from_bytes(source_hash[:8], "little"),
    )

    background = np.asarray(map_data.background, dtype="<u4")
    foreground = np.asarray(map_data.foreground, dtype="<u4")
    collision = np.asarray(map_data.collision, dtype="u1")

    with open(output, "wb") as f:
        f.write(header)
        background.tofile(f)
        foreground.tofile(f)
        collision.tofile(f)
        cc.astype("<u2", copy=False).reshape(-1).tofile(f)
        for d in range(8):
            jump[d].astype("<i2", copy=False).tofile(f)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name", help="Tilemap name (without extension).")
    args = parser.parse_args()

    name = args.name
    source_path = os.path.join(SOURCE_DIR, f"{name}.tmx")
    if not os.path.isfile(source_path):
        print(f"Not found: {source_path}", file=sys.stderr)
        return 1

    map_data = parse_tmx(source_path, name)
    width, height = map_data.width, map_data.height
    if width * height > 4096 * 4096:
        raise SystemExit(f"map {width}x{height} exceeds 4096x4096 budget")

    raw = np.array(map_data.collision, dtype=np.uint8).reshape(height, width)
    raw_blocked = raw != 0
    blocked = inflate(raw_blocked, map_data.radius_tiles)

    walkable = int((~blocked).sum())
    print(
        f"[{name}] {width}x{height} cells, "
        f"radius_tiles={map_data.radius_tiles}, walkable={walkable}/{width * height}"
    )

    cc = connected_components(blocked)
    jump = compute_jump_tables(blocked)

    components = int(cc[cc != 0xFFFF].max()) if walkable else 0
    print(f"[{name}] components={components}")

    with open(source_path, "rb") as f:
        source_hash = hashlib.blake2b(f.read(), digest_size=8).digest()

    os.makedirs(RUNTIME_DIR, exist_ok=True)
    map_path = os.path.join(RUNTIME_DIR, f"{name}.map")

    emit_map(map_path, map_data, cc, jump, source_hash)
    expected_bytes = 24 + 27 * width * height
    actual_bytes = os.path.getsize(map_path)
    if actual_bytes != expected_bytes:
        raise SystemExit(
            f"{map_path}: produced {actual_bytes} bytes, expected {expected_bytes}"
        )
    print(f"Wrote {map_path} ({actual_bytes} bytes)")

    blob_dir = os.path.join(BLOBS_DIR, name)
    os.makedirs(blob_dir, exist_ok=True)
    for ts in map_data.tilesets:
        if not os.path.isfile(ts.image):
            print(f"Warning: tileset image missing: {ts.image} (skipping)")
            continue
        target = os.path.join(blob_dir, f"{ts.name}.png")
        shutil.copy2(ts.image, target)
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
