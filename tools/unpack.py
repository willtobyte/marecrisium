#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.13"
# dependencies = ["zstandard"]
# ///

import importlib
import struct
import sys
from pathlib import Path

zstandard = importlib.import_module("zstandard")

MAGIC = b"CRO2"
DIRECTORY = 2
ALGO_RAW = 0
HEADER_FORMAT = "<4s5I40x"
RECORD_FORMAT = "<3IH2B"
HEADER = struct.calcsize(HEADER_FORMAT)
RECORD = struct.calcsize(RECORD_FORMAT)


def main() -> int:
    rom = memoryview(Path("cartridge.rom").read_bytes())

    magic, count, stringsize, trainsize, slots, _seed = struct.unpack_from(
        HEADER_FORMAT, rom, 0
    )

    if magic != MAGIC:
        print("not a cartridge.rom", file=sys.stderr)
        return 1

    cursor = HEADER + slots * 2
    records = rom[cursor : cursor + count * RECORD]
    cursor += count * RECORD
    strings = rom[cursor : cursor + stringsize]
    cursor += stringsize
    trained = rom[cursor : cursor + trainsize]

    dictionary = zstandard.ZstdCompressionDict(trained)
    decoder = zstandard.ZstdDecompressor(dict_data=dictionary)

    root = Path("cartridge").resolve()

    for (
        position,
        compressed,
        uncompressed,
        offset,
        length,
        kind,
    ) in struct.iter_unpack(RECORD_FORMAT, records):
        path = Path(bytes(strings[offset : offset + length]).decode("utf-8"))
        if path.anchor or ".." in path.parts:
            raise ValueError("cartridge entry path must be relative")
        destination = (root / path).resolve()
        if not destination.is_relative_to(root):
            raise ValueError("cartridge entry path must be inside cartridge")

        if kind == DIRECTORY:
            destination.mkdir(parents=True, exist_ok=True)
            continue

        destination.parent.mkdir(parents=True, exist_ok=True)

        if uncompressed == 0:
            destination.write_bytes(b"")
            continue

        if kind == ALGO_RAW:
            destination.write_bytes(rom[position : position + uncompressed])
            continue

        destination.write_bytes(
            decoder.decompress(
                rom[position : position + compressed], max_output_size=uncompressed
            )
        )

    print(f"extracted {count} entries to cartridge/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
