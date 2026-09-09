# /// script
# requires-python = ">=3.14"
# dependencies = ["pillow", "pyoxipng"]
# ///

import argparse
import importlib
import math
import re
from pathlib import Path

import tomllib

Image = importlib.import_module("PIL.Image")
ImageColor = importlib.import_module("PIL.ImageColor")
ImageDraw = importlib.import_module("PIL.ImageDraw")
ImageFont = importlib.import_module("PIL.ImageFont")
oxipng = importlib.import_module("oxipng")

parser = argparse.ArgumentParser(
    description="Create all bitmap fonts from assets/fonts/*/config.toml."
)
parser.parse_args()
root = Path(__file__).resolve().parents[2]

for path in sorted((root / "assets/fonts").glob("*/config.toml")):
    try:
        with path.open("rb") as stream:
            config = tomllib.load(stream)
        name = config["name"]
        glyphs = config["glyphs"]
        size = config.get("size", 16)
        shadow = config.get("shadow", False)
        color = ImageColor.getcolor(config.get("color", "black"), "RGBA")
        shade = ImageColor.getcolor(config.get("shadow_color", "#00000040"), "RGBA")

        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", name):
            raise ValueError(
                "Use letters, digits, hyphens, or underscores for the name."
            )

        if not glyphs or any(not 32 <= ord(glyph) <= 126 for glyph in glyphs):
            raise ValueError(
                "Use printable ASCII glyphs. The font loader reads one byte per glyph."
            )

        if size <= 0:
            raise ValueError("The font size must be positive.")

        font = ImageFont.FreeTypeFont(
            path.parent / config["font"], size, layout_engine=ImageFont.Layout.BASIC
        )
    except (KeyError, OSError, ValueError) as error:
        parser.error(f"{path}: {error}")

    glyphs = "".join(dict.fromkeys(glyphs))
    ascent, descent = font.getmetrics()
    cell = 1
    offsets = []
    for glyph in glyphs:
        left, top, right, bottom = font.getbbox(glyph, mode="1", anchor="ls")
        left = min(0, left)
        cell = max(
            cell, max(1, math.ceil(font.getlength(glyph, mode="1")), right) - left
        )
        offsets.append(left)
        ascent = max(ascent, -top)
        descent = max(descent, bottom)

    separator = next(
        value
        for value in ((255, 255, 0, 255), (255, 0, 255, 255), (0, 255, 255, 255))
        if value not in (color, shade)
    )
    cell += shadow
    width = 1 + (cell + 1) * len(glyphs)
    height = max(1, ascent + descent) + shadow
    sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(sheet)
    draw.fontmode = "1"
    draw.line((0, 0, 0, height - 1), fill=separator)
    x = 1

    for glyph, left in zip(glyphs, offsets):
        if shadow:
            draw.text(
                (x - left + 1, ascent + 1),
                glyph,
                font=font,
                anchor="ls",
                fill=shade,
            )
        draw.text((x - left, ascent), glyph, font=font, anchor="ls", fill=color)
        x += cell
        draw.line((x, 0, x, height - 1), fill=separator)
        x += 1

    atlas = root / "cartridge/blobs/fonts" / f"{name}.png"
    output = root / "cartridge/fonts" / f"{name}.lua"
    atlas.parent.mkdir(parents=True, exist_ok=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    image = oxipng.RawImage(sheet.tobytes(), width, height)
    sheet.close()
    atlas.write_bytes(
        image.create_optimized_png(
            level=6,
            strip=oxipng.StripChunks.all(),
            deflate=oxipng.Deflaters.zopfli(15),
        )
    )
    escaped = glyphs.replace("\\", "\\\\").replace('"', '\\"')
    output.write_text(f'return {{\n\tglyphs = "{escaped}",\n}}\n', encoding="utf-8")

    print(f"{name} ({len(glyphs)} glyphs) '{glyphs}'")
