#!/usr/bin/env python3

from __future__ import annotations

import argparse
import base64
import math
import os
import struct
import sys
import xml.etree.ElementTree as ET
import zlib

FLIPPED_HORIZONTALLY_FLAG = 0x80000000
FLIPPED_VERTICALLY_FLAG = 0x40000000
FLIPPED_DIAGONALLY_FLAG = 0x20000000
ROTATED_HEXAGONAL_120_FLAG = 0x10000000
ALL_FLIP_FLAGS = (
    FLIPPED_HORIZONTALLY_FLAG
    | FLIPPED_VERTICALLY_FLAG
    | FLIPPED_DIAGONALLY_FLAG
    | ROTATED_HEXAGONAL_120_FLAG
)


def _strip_flip_bits(gid: int) -> int:
    return gid & ~ALL_FLIP_FLAGS


def _decode_layer_data(data_element: ET.Element, expected: int) -> list[int]:
    encoding = (data_element.get("encoding") or "").strip().lower()
    compression = (data_element.get("compression") or "").strip().lower()

    if encoding == "csv":
        text = data_element.text or ""
        raw = [int(v.strip()) for v in text.split(",") if v.strip()]
        return [_strip_flip_bits(g) for g in raw]

    if encoding == "base64":
        raw_bytes = base64.b64decode(data_element.text or "")
        if compression == "zlib":
            raw_bytes = zlib.decompress(raw_bytes)
        elif compression == "gzip":
            raw_bytes = zlib.decompress(raw_bytes, zlib.MAX_WBITS | 16)
        elif compression:
            raise ValueError(f"Unsupported compression: {compression}")
        count = len(raw_bytes) // 4
        gids = struct.unpack(f"<{count}I", raw_bytes)
        return [_strip_flip_bits(g) for g in gids]

    tiles = data_element.findall("tile")
    return [_strip_flip_bits(int(t.get("gid", "0"))) for t in tiles]


def _find_tileset_for_gid(
    gid: int, tilesets: list[tuple[int, int]]
) -> tuple[int, int] | None:
    result = None
    for firstgid, tilecount in tilesets:
        if gid >= firstgid:
            result = (firstgid, tilecount)
        else:
            break
    return result


def convert(tmx_path: str, output_dir: str) -> None:
    tree = ET.parse(tmx_path)
    root = tree.getroot()

    stage_name = os.path.splitext(os.path.basename(tmx_path))[0]

    map_width = int(root.get("width", "0"))
    map_height = int(root.get("height", "0"))
    tile_width = int(root.get("tilewidth", "0"))
    tile_height = int(root.get("tileheight", "0"))

    if tile_width != tile_height:
        print(
            f"Warning: non-square tiles ({tile_width}x{tile_height}), using width",
            file=sys.stderr,
        )
    tile_size = tile_width

    total = map_width * map_height
    if total == 0:
        print("Error: map has zero tiles", file=sys.stderr)
        sys.exit(1)

    tilesets: list[tuple[int, int]] = []
    for ts in root.findall("tileset"):
        firstgid = int(ts.get("firstgid", "1"))
        tilecount = int(ts.get("tilecount", "0"))
        tilesets.append((firstgid, tilecount))
    tilesets.sort(key=lambda t: t[0])

    background_gids: list[int] | None = None
    foreground_gids: list[int] | None = None
    collision_gids: list[int] | None = None

    for layer in root.findall("layer"):
        name = (layer.get("name") or "").strip().lower()
        data_el = layer.find("data")
        if data_el is None:
            continue

        gids = _decode_layer_data(data_el, total)

        if name == "background":
            background_gids = gids
        elif name == "foreground":
            foreground_gids = gids
        elif name == "collision":
            collision_gids = gids

    objects: list[dict[str, str | float]] = []
    for objgroup in root.findall("objectgroup"):
        group_name = (objgroup.get("name") or "").strip().lower()
        if group_name != "objects":
            continue
        for obj in objgroup.findall("object"):
            obj_name = (obj.get("name") or "").strip()
            obj_kind = (obj.get("class") or obj.get("type") or "").strip()
            obj_x = float(obj.get("x", "0"))
            obj_y = float(obj.get("y", "0"))
            if obj_name and obj_kind:
                objects.append(
                    {"name": obj_name, "kind": obj_kind, "x": obj_x, "y": obj_y}
                )

    def gids_to_local(gids: list[int]) -> list[int]:
        result = []
        for gid in gids:
            if gid == 0:
                result.append(0)
                continue
            ts = _find_tileset_for_gid(gid, tilesets)
            if ts is None:
                result.append(0)
                continue
            firstgid, _ = ts
            local_id = gid - firstgid + 1
            result.append(local_id)
        return result

    bg_tiles = gids_to_local(background_gids) if background_gids else [0] * total
    fg_tiles = gids_to_local(foreground_gids) if foreground_gids else [0] * total
    col_tiles = (
        [1 if g != 0 else 0 for g in collision_gids] if collision_gids else [0] * total
    )

    for label, arr in [
        ("background", bg_tiles),
        ("foreground", fg_tiles),
        ("collision", col_tiles),
    ]:
        if len(arr) != total:
            print(
                f"Warning: {label} has {len(arr)} tiles, expected {total}",
                file=sys.stderr,
            )
            if len(arr) < total:
                arr.extend([0] * (total - len(arr)))
            else:
                del arr[total:]

    tilemaps_dir = os.path.join(output_dir, "tilemaps")
    os.makedirs(tilemaps_dir, exist_ok=True)

    tilemap_path = os.path.join(tilemaps_dir, f"{stage_name}.lua")
    with open(tilemap_path, "w", newline="\n") as f:
        f.write("return {\n")
        f.write(f"\tsize = {tile_size},\n")
        f.write(f"\twidth = {map_width},\n")
        f.write(f"\theight = {map_height},\n")

        for layer_name, tiles in [
            ("background", bg_tiles),
            ("foreground", fg_tiles),
            ("collision", col_tiles),
        ]:
            f.write(f"\t{layer_name} = {{\n")
            for tile in tiles:
                f.write(f"\t\t{tile},\n")
            f.write("\t},\n")

        f.write("}\n")

    print(f"  tilemaps/{stage_name}.lua ({total} tiles per layer)")

    stages_dir = os.path.join(output_dir, "stages")
    os.makedirs(stages_dir, exist_ok=True)

    stage_path = os.path.join(stages_dir, f"{stage_name}.lua")
    with open(stage_path, "w", newline="\n") as f:
        f.write('local ticker = require("helpers/ticker")\n')
        f.write("\n")
        f.write("return ticker.wrap({\n")
        f.write(f'\ttilemap = "{stage_name}",\n')

        if objects:
            f.write("\n")
            f.write("\tobjects = {\n")
            for obj in objects:
                f.write(f'\t\t{{ name = "{obj["name"]}", kind = "{obj["kind"]}" }},\n')
            f.write("\t},\n")

            f.write("\n")
            f.write("\ton_enter = function()\n")
            for obj in objects:
                ox = float(obj["x"])
                oy = float(obj["y"])
                x: int | float = int(ox) if ox == math.floor(ox) else ox
                y: int | float = int(oy) if oy == math.floor(oy) else oy
                f.write(f"\t\tpool.{obj['name']}.position = {{ {x}, {y} }}\n")
            f.write("\tend,\n")

        f.write("})\n")

    print(f"  stages/{stage_name}.lua ({len(objects)} objects)")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert a Tiled .tmx map into Carimbo engine Lua files.",
    )
    parser.add_argument("tmx", help="Path to the .tmx file")
    parser.add_argument(
        "--output-dir",
        default=".",
        help="Base output directory (default: current directory)",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.tmx):
        print(f"Error: file not found: {args.tmx}", file=sys.stderr)
        sys.exit(1)

    print(f"Converting {args.tmx}...")
    convert(args.tmx, args.output_dir)
    print("Done.")


if __name__ == "__main__":
    main()
