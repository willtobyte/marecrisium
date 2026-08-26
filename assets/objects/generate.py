#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.14"
# dependencies = ["jinja2", "pillow", "pyoxipng", "rectangle-packer"]
# ///

import importlib
import re
from pathlib import Path
from shutil import copy2

Image = importlib.import_module("PIL.Image")
jinja2 = importlib.import_module("jinja2")
oxipng = importlib.import_module("oxipng")
rpack = importlib.import_module("rpack")

objects = Path(__file__).resolve().parent
root = objects.parents[1]
pattern = re.compile(r"^(\d+)_(\d+)_([a-z][a-z0-9.]*)\.png$")

for directory in sorted(path for path in objects.iterdir() if path.is_dir()):
    paths = sorted((directory / "frames").glob("*.png"))
    if not paths:
        continue

    groups = {}
    images = []

    for path in paths:
        match = pattern.fullmatch(path.name)
        assert match, "frame name must be order_duration_animation.png"
        order, duration, animation = match.groups()
        source = Image.open(path)
        image = source if source.mode == "RGBA" else source.convert("RGBA")
        if image is not source:
            source.close()

        collider = directory / "colliders" / path.name
        assert collider.is_file(), "frame must have a collider mask"
        with Image.open(collider) as mask:
            assert mask.size == image.size, "collider mask size must match frame size"
            assert "A" in mask.getbands(), "collider mask must have an alpha channel"
            alpha = mask.getchannel("A")
            bounds = alpha.getbbox()
            alpha.close()
        assert bounds, "collider mask must have opaque pixels"
        cx, cy, right, bottom = bounds
        images.append(
            (
                image,
                animation,
                int(order),
                int(duration),
                (cx, cy, right - cx, bottom - cy),
            )
        )

    sizes = [image.size for image, _, _, _, _ in images]
    positions = rpack.pack(sizes)
    width = max(x + w for (x, _), (w, _) in zip(positions, sizes))
    height = max(y + h for (_, y), (_, h) in zip(positions, sizes))
    sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))

    for (image, animation, order, duration, collider), (x, bottom) in zip(
        images, positions
    ):
        w, h = image.size
        y = height - bottom - h
        sheet.paste(image, (x, y))
        cx, cy, cw, ch = collider
        frame = ", ".join(map(str, (x, y, w, h, duration, cx, cy, cw, ch)))
        groups.setdefault(animation, []).append((order, frame))
        image.close()

    assert len(groups) <= 255, "object must have at most 255 animations"
    assert len(images) <= 65535, "object must have at most 65535 frames"
    clips = []
    for animation, frames in sorted(groups.items()):
        frames.sort(key=lambda item: item[0])
        assert len(frames) <= 255, "animation must have at most 255 frames"
        assert len({order for order, _ in frames}) == len(frames), (
            "animation frame order must be unique"
        )
        clips.append({"name": animation, "frames": [frame for _, frame in frames]})

    name = directory.name
    atlas = directory / f"{name}.png"
    sheet.save(atlas, compress_level=0)
    sheet.close()
    oxipng.optimize(
        atlas,
        level=6,
        filter=[oxipng.RowFilter.Brute],
        strip=oxipng.StripChunks.all(),
        interlace=oxipng.Interlacing.Off,
    )

    patch = directory / "patch.lua.j2"
    environment = jinja2.Environment(
        loader=jinja2.FileSystemLoader([directory, objects]),
        keep_trailing_newline=True,
    )
    template = environment.get_template(
        patch.name if patch.is_file() else "template.lua.j2"
    )

    output = root / "cartridge" / "objects" / f"{name}.lua"
    destination = root / "cartridge" / "blobs" / "objects" / f"{name}.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    destination.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(template.render(clips=clips).lstrip("\n"))
    copy2(atlas, destination)
