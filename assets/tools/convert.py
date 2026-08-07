#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.13"
# dependencies = ["jinja2", "pyoxipng"]
# ///
"""Convert TexturePacker JSON output into a Lua object."""

import importlib
import json
import os
import re
import sys
from pathlib import Path
from shutil import copy2

try:
    jinja2 = importlib.import_module("jinja2")
    oxipng = importlib.import_module("oxipng")
except ModuleNotFoundError:
    os.execvp(
        "uv",
        ["uv", "run", "--script", str(Path(__file__).resolve()), *sys.argv[1:]],
    )


def frame(data, duration):
    packed = data["frame"]
    source = data["sourceSize"]
    width, height = source["w"], source["h"]
    cw = max(1, round(width * 0.5))
    ch = max(1, round(height * 0.3))
    cx = (width - cw) // 2
    cy = height - ch
    return ", ".join(
        map(
            str,
            (
                packed["x"],
                packed["y"],
                packed["w"],
                packed["h"],
                duration,
                cx,
                cy,
                cw,
                ch,
            ),
        )
    )


source = Path(sys.argv[1]).resolve()
name = source.stem
directory = source.parent
sheet = directory / f"{name}.png"
config = directory / "config.json"
root = Path(__file__).resolve().parents[2]
output = root / "cartridge" / "objects" / f"{name}.lua"
destination = root / "cartridge" / "blobs" / "objects" / f"{name}.png"

with source.open() as file:
    frames = json.load(file)["frames"]

groups = {}
for name, data in frames.items():
    if match := re.match(r"^(.+?)\s+(\d+)\.png$", name):
        groups.setdefault(match[1], []).append((int(match[2]), data))
groups = {
    name: sorted(frames, key=lambda data: data[0])
    for name, frames in sorted(groups.items())
}

if config.is_file():
    with config.open() as file:
        settings = json.load(file)
else:
    settings = {
        "duration": {name: [100] * len(frames) for name, frames in groups.items()},
        "aliases": {},
    }

    config.write_text(f"{json.dumps(settings, indent=2)}\n")

durations = settings["duration"]
clips = []
for name, frames in groups.items():
    clips.append(
        {
            "name": name,
            "frames": [
                frame(data, durations[name][index])
                for index, (_, data) in enumerate(frames)
            ],
        }
    )

for name, alias in sorted(settings.get("aliases", {}).items()):
    group = alias["group"]
    clips.append(
        {
            "name": name,
            "frames": [
                frame(groups[group][index][1], durations[group][index])
                for index in alias["frames"]
            ],
        }
    )

patch = directory / "patch.lua.j2"
environment = jinja2.Environment(
    loader=jinja2.FileSystemLoader([directory, root / "assets" / "objects"]),
    keep_trailing_newline=True,
)

template = environment.get_template(
    patch.name if patch.is_file() else "template.lua.j2"
)

output.parent.mkdir(parents=True, exist_ok=True)
destination.parent.mkdir(parents=True, exist_ok=True)
output.write_text(template.render(clips=clips).lstrip("\n"))

copy2(sheet, destination)

oxipng.optimize(
    destination,
    level=6,
    filter=[oxipng.RowFilter.Brute],
    strip=oxipng.StripChunks.safe(),
    interlace=oxipng.Interlacing.Off,
)
