#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.14"
# dependencies = ["jinja2", "pillow", "rectangle-packer"]
# ///

import importlib
import math
import re
from pathlib import Path

Image = importlib.import_module("PIL.Image")
jinja2 = importlib.import_module("jinja2")
rpack = importlib.import_module("rpack")

objects = Path(__file__).resolve().parent
root = objects.parents[1]
pattern = re.compile(r"^(\d+)_(\d+)_(?:(end)_)?([a-z][a-z0-9.]*)\.png$")

for directory in sorted(path for path in objects.iterdir() if path.is_dir()):
    paths = sorted((directory / "frames").glob("*.png"))
    if not paths:
        continue

    groups = {}
    images = []

    for path in paths:
        match = pattern.fullmatch(path.name)
        assert match, "frame name must be index_delay_[end_]animation.png"
        order, duration, end, animation = match.groups()
        order = int(order)
        duration = int(duration)
        group = groups.get(animation)
        if group is None:
            groups[animation] = (order if end else None, [])
        else:
            assert not end or group[0] is None, "animation must have one end marker"
            if end:
                groups[animation] = (order, group[1])
        source = Image.open(path)
        image = source if source.mode == "RGBA" else source.convert("RGBA")
        if image is not source:
            source.close()

        size = image.size
        assert max(size) <= 65535, "source frame must fit within 65535 pixels"
        bounds = image.getbbox(alpha_only=True)
        assert bounds, "frame must have opaque pixels"

        collider = directory / "colliders" / path.name
        if collider.is_file():
            with Image.open(collider) as mask:
                assert mask.size == size, "collider mask size must match frame size"
                assert mask.has_transparency_data, (
                    "collider mask must have transparency data"
                )
                if "A" in mask.getbands():
                    box = mask.getbbox(alpha_only=True)
                else:
                    with mask.convert("RGBA") as image:
                        box = image.getbbox(alpha_only=True)
            assert box, "collider mask must have opaque pixels"
            cx, cy, right, bottom = box
            collider = (cx, cy, right - cx, bottom - cy)
        else:
            collider = None
        images.append(
            (
                image,
                animation,
                order,
                duration,
                collider,
                bounds,
            )
        )

    size = images[0][0].size
    consistent = all(image.size == size for image, _, _, _, _, _ in images)
    assert consistent, "object frames must have the same size"

    for slot, (image, animation, order, duration, collider, bounds) in enumerate(
        images
    ):
        left, top, _, _ = bounds
        if bounds != (0, 0, *size):
            crop = image.crop(bounds)
            image.close()
            image = crop
        if collider:
            cx, cy, cw, ch = collider
            cx -= left
            cy -= top
            collider = (cx, cy, cw, ch)
        images[slot] = (
            image,
            animation,
            order,
            duration,
            collider,
            (left, top),
        )

    sizes = [image.size for image, _, _, _, _, _ in images]
    area = sum(w * h for w, h in sizes)
    side = max(max(max(size) for size in sizes), math.isqrt(area - 1) + 1)
    side = 1 << (side - 1).bit_length()
    while True:
        assert side <= 16384, "object atlas must fit within 16384 pixels"
        try:
            positions = rpack.pack(sizes, max_width=side, max_height=side)
            break
        except rpack.PackingImpossibleError:
            side <<= 1
    width = max(x + w for (x, _), (w, _) in zip(positions, sizes))
    height = max(y + h for (_, y), (_, h) in zip(positions, sizes))
    sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))

    for (image, animation, order, duration, collider, offset), (x, bottom) in zip(
        images, positions
    ):
        w, h = image.size
        y = height - bottom - h
        sheet.paste(image, (x, y))
        left, top = offset
        frame = f"{x}, {y}, {w}, {h}, {left}, {top}, {size[0]}, {size[1]}, {duration}"
        if collider:
            frame += ", " + ", ".join(map(str, collider))
        groups[animation][1].append((order, frame))
        image.close()

    assert len(groups) <= 255, "object must have at most 255 animations"
    assert len(images) <= 65535, "object must have at most 65535 frames"
    clips = []
    for animation, (end, frames) in sorted(groups.items()):
        frames.sort(key=lambda item: item[0])
        assert len(frames) <= 255, "animation must have at most 255 frames"
        assert len({order for order, _ in frames}) == len(frames), (
            "animation frame order must be unique"
        )

        assert end is None or end == frames[-1][0], (
            "end marker must be on the last animation frame"
        )

        clips.append(
            {
                "name": animation,
                "loop": end is None,
                "frames": [frame for _, frame in frames],
            }
        )

    name = directory.name
    output = root / "cartridge" / "objects" / f"{name}.lua"
    atlas = root / "cartridge" / "blobs" / "objects" / f"{name}.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    atlas.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(atlas, compress_level=0)
    sheet.close()

    patch = directory / "patch.lua.j2"
    environment = jinja2.Environment(
        loader=jinja2.FileSystemLoader([directory, objects]),
        keep_trailing_newline=True,
    )
    template = environment.get_template(
        patch.name if patch.is_file() else "template.lua.j2"
    )

    output.write_text(template.render(clips=clips).lstrip("\n"))
