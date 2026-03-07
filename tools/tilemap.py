#!/usr/bin/env python3

import argparse
import os
import shutil
import sys
import xml.etree.ElementTree as ET

FLIP_MASK = 0x1FFFFFFF


def parse_tileset(map_element, tmx_directory):
    tileset_element = map_element.find("tileset")
    if tileset_element is None:
        print("error: no <tileset> found in TMX", file=sys.stderr)
        sys.exit(1)

    tsx_source = tileset_element.get("source")
    if tsx_source is not None:
        tsx_path = os.path.join(tmx_directory, tsx_source)
        tsx_tree = ET.parse(tsx_path)
        tsx_root = tsx_tree.getroot()
        image_element = tsx_root.find("image")
        image_directory = os.path.dirname(tsx_path)
    else:
        image_element = tileset_element.find("image")
        image_directory = tmx_directory

    if image_element is None:
        print("error: no <image> found in tileset", file=sys.stderr)
        sys.exit(1)

    source = image_element.get("source", "")
    return os.path.join(image_directory, source)


def parse_layer_data(layer_element):
    data_element = layer_element.find("data")
    if data_element is None:
        return []

    encoding = data_element.get("encoding")

    if encoding == "csv":
        text = data_element.text.strip()
        return [int(value) for value in text.split(",")]

    if encoding is None:
        tiles = data_element.findall("tile")
        return [int(tile.get("gid", "0")) for tile in tiles]

    print(f"error: unsupported encoding '{encoding}'", file=sys.stderr)
    sys.exit(1)


def format_tile_array(values, indent="\t"):
    if not values:
        return "{}"

    lines = [f"{indent}\t{value}," for value in values]
    return "{\n" + "\n".join(lines) + "\n" + indent + "}"


def convert(input_path, output_path):
    tmx_directory = os.path.dirname(os.path.abspath(input_path))

    tree = ET.parse(input_path)
    root = tree.getroot()

    columns = int(root.get("width", "0"))
    rows = int(root.get("height", "0"))
    tile_size = int(root.get("tilewidth", "0"))

    tileset_image_path = parse_tileset(root, tmx_directory)

    output_name, _ = os.path.splitext(os.path.basename(output_path))
    output_directory = os.path.dirname(os.path.abspath(output_path))
    blobs_directory = os.path.join(output_directory, os.pardir, "blobs", "tilemaps")
    os.makedirs(blobs_directory, exist_ok=True)

    destination = os.path.join(blobs_directory, f"{output_name}.png")
    shutil.copy2(tileset_image_path, destination)
    print(f"copied {tileset_image_path} -> {destination}")

    layers = {}
    for layer in root.findall("layer"):
        name = layer.get("name", "").lower()
        layers[name] = parse_layer_data(layer)

    background = [gid & FLIP_MASK for gid in layers.get("background", [])]
    foreground = [gid & FLIP_MASK for gid in layers.get("foreground", [])]
    collision_raw = layers.get("collision", [])
    collision = [0 if (gid & FLIP_MASK) == 0 else 1 for gid in collision_raw]

    with open(output_path, "w", newline="\n") as file:
        file.write("return {\n")
        file.write(f"\ttile = {tile_size},\n")
        file.write(f"\tcolumns = {columns},\n")
        file.write(f"\trows = {rows},\n")
        file.write(f"\n\tbackground = {format_tile_array(background)},\n")
        file.write(f"\n\tforeground = {format_tile_array(foreground)},\n")
        file.write(f"\n\tcollision = {format_tile_array(collision)},\n")
        file.write("}\n")


def main():
    parser = argparse.ArgumentParser(
        description="Convert TMX tilemap to Carimbo Lua format"
    )
    parser.add_argument("input", help="Path to input .tmx file")
    parser.add_argument("output", help="Path to output .lua file")
    args = parser.parse_args()

    if not os.path.isfile(args.input):
        print(f"error: file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    convert(args.input, args.output)
    print(f"converted {args.input} -> {args.output}")


if __name__ == "__main__":
    main()
