#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.13"
# dependencies = ["jinja2", "pyoxipng"]
# ///
"""Generate a Lua object file from TexturePacker JSON output."""

import json
import os
import re
import shutil
import sys

from jinja2 import Environment, FileSystemLoader
import oxipng

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.normpath(os.path.join(here, ".."))


def load(path):
    with open(path) as f:
        data = json.load(f)
    return data["frames"]


def group(frames):
    groups = {}
    for name, info in frames.items():
        match = re.match(r"^(.+?)\s+(\d+)\.png$", name)
        if not match:
            continue
        prefix = match.group(1)
        index = int(match.group(2))
        groups.setdefault(prefix, []).append((index, info))
    for prefix in groups:
        groups[prefix].sort(key=lambda pair: pair[0])
    return groups


def scaffold(groups, path):
    config = {
        "body": "dynamic",
        "duration": {},
        "aliases": {},
    }
    for prefix in sorted(groups.keys()):
        count = len(groups[prefix])
        config["duration"][prefix] = [100] * count
    with open(path, "w") as f:
        json.dump(config, f, indent=2)
        f.write("\n")
    print(f"Generated {path}")
    return config


def load_config(path):
    with open(path) as f:
        return json.load(f)


def collider(info, duration):
    f = info["frame"]
    w, h = f["w"], f["h"]
    cw = max(1, round(w * 0.5))
    ch = max(1, round(h * 0.3))
    cx = (w - cw) // 2
    cy = h - ch
    return f"{f['x']}, {f['y']}, {w}, {h}, {duration}, {cx}, {cy}, {cw}, {ch}"


def clips(groups, config):
    durations = config["duration"]
    result = []

    for prefix in sorted(groups.keys()):
        frames = groups[prefix]
        if prefix not in durations:
            print(
                f"Error: group '{prefix}' not in config.json duration", file=sys.stderr
            )
            sys.exit(1)
        pd = durations[prefix]
        if len(pd) != len(frames):
            print(
                f"Error: group '{prefix}' has {len(frames)} frames but config has {len(pd)} durations",
                file=sys.stderr,
            )
            sys.exit(1)
        result.append(
            {
                "name": prefix,
                "frames": [collider(info, pd[i]) for i, (_, info) in enumerate(frames)],
            }
        )

    for name, alias in sorted(config.get("aliases", {}).items()):
        source = alias["group"]
        indices = alias["frames"]
        if source not in groups:
            print(
                f"Error: alias '{name}' references unknown group '{source}'",
                file=sys.stderr,
            )
            sys.exit(1)
        frames = groups[source]
        pd = durations[source]
        for i in indices:
            if i >= len(frames):
                print(
                    f"Error: alias '{name}' references frame {i} but group '{source}' has {len(frames)} frames",
                    file=sys.stderr,
                )
                sys.exit(1)
        result.append(
            {
                "name": name,
                "frames": [collider(frames[i][1], pd[i]) for i in indices],
            }
        )

    return result


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <name>", file=sys.stderr)
        sys.exit(1)

    name = sys.argv[1]
    directory = os.path.join(here, name)
    source = os.path.join(directory, f"{name}.json")
    sheet = os.path.join(directory, f"{name}.png")
    output = os.path.join(root, "cartridge", "objects", f"{name}.lua")
    destination = os.path.join(root, "cartridge", "blobs", "objects", f"{name}.png")
    configpath = os.path.join(directory, "config.json")

    if not os.path.isfile(source):
        print(f"Not found: {source}", file=sys.stderr)
        sys.exit(1)

    frames = load(source)
    groups = group(frames)

    if not groups:
        print(f"No animation groups found in {source}", file=sys.stderr)
        sys.exit(1)

    print(f"Found groups: {', '.join(sorted(groups.keys()))}")

    if os.path.isfile(configpath):
        config = load_config(configpath)
        print(f"Loaded {configpath}")
    else:
        config = scaffold(groups, configpath)

    environment = Environment(
        loader=FileSystemLoader([directory, here]),
        keep_trailing_newline=True,
    )

    patch = os.path.join(directory, "patch.lua.j2")
    template = "patch.lua.j2" if os.path.isfile(patch) else "template.lua.j2"
    rendered = (
        environment.get_template(template)
        .render(
            body=config["body"],
            clips=clips(groups, config),
        )
        .lstrip("\n")
    )

    with open(output, "w") as f:
        f.write(rendered)
    print(f"Wrote {output}")

    shutil.copy2(sheet, destination)
    print(f"Copied spritesheet to {destination}")

    oxipng.optimize(
        destination,
        level=6,
        filter=[oxipng.RowFilter.Brute],
        strip=oxipng.StripChunks.safe(),
        interlace=oxipng.Interlacing.Off,
    )
    print(f"Optimized {destination}")


if __name__ == "__main__":
    main()
