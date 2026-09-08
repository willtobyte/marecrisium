# /// script
# requires-python = ">=3.14"
# dependencies = ["pillow", "pyoxipng"]
# ///

import argparse
import importlib
import math
import re
from pathlib import Path

Image = importlib.import_module("PIL.Image")
ImageDraw = importlib.import_module("PIL.ImageDraw")
ImageFont = importlib.import_module("PIL.ImageFont")
oxipng = importlib.import_module("oxipng")

parser = argparse.ArgumentParser(description="Create a bitmap font PNG and Lua file.")
parser.add_argument(
    "--name", required=True, help="Output name without a file extension."
)
parser.add_argument(
    "--glyphs", required=True, help="Printable ASCII glyphs in atlas order."
)
parser.add_argument(
    "--font", type=Path, required=True, help="Path to a font file in assets."
)
parser.add_argument(
    "--size", type=int, default=16, help="Font size in pixels (default: 16)."
)
parser.add_argument(
    "--shadow",
    action="store_true",
    help="Add a black shadow with 25%% opacity one pixel right and down.",
)
args = parser.parse_args()

if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", args.name):
    parser.error("Use letters, digits, hyphens, or underscores for the output name.")

if not args.glyphs or any(not 32 <= ord(glyph) <= 126 for glyph in args.glyphs):
    parser.error(
        "Use printable ASCII glyphs. The font loader reads one byte per glyph."
    )

if args.size <= 0:
    parser.error("The font size must be positive.")

glyphs = "".join(dict.fromkeys(args.glyphs))
try:
    font = ImageFont.FreeTypeFont(
        args.font, args.size, layout_engine=ImageFont.Layout.BASIC
    )
except OSError:
    parser.error(f"Cannot load font file: {args.font}")

ascent, descent = font.getmetrics()
cell = 1
offsets = []
for glyph in glyphs:
    left, top, right, bottom = font.getbbox(glyph, mode="1", anchor="ls")
    left = min(0, left)
    cell = max(cell, max(1, math.ceil(font.getlength(glyph, mode="1")), right) - left)
    offsets.append(left)
    ascent = max(ascent, -top)
    descent = max(descent, bottom)

cell += args.shadow
width = 1 + (cell + 1) * len(glyphs)
height = max(1, ascent + descent) + args.shadow
sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))
draw = ImageDraw.Draw(sheet)
draw.fontmode = "1"
draw.line((0, 0, 0, height - 1), fill=(255, 255, 0, 255))
x = 1

for glyph, left in zip(glyphs, offsets):
    if args.shadow:
        draw.text(
            (x - left + 1, ascent + 1),
            glyph,
            font=font,
            anchor="ls",
            fill=(0, 0, 0, 64),
        )
    draw.text((x - left, ascent), glyph, font=font, anchor="ls", fill="black")
    x += cell
    draw.line((x, 0, x, height - 1), fill=(255, 255, 0, 255))
    x += 1

root = Path(__file__).resolve().parents[2] / "cartridge"
atlas = root / "blobs/fonts" / f"{args.name}.png"
output = root / "fonts" / f"{args.name}.lua"
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

print(atlas)
print(output)
