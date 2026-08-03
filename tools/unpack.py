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

MAGIC = 0x4D4F5243
DIRECTORY = 1
ALGO_RAW = 0
HEADER_FORMAT = "<9I"
RECORD_FORMAT = "<4IH2B"
HEADER = struct.calcsize(HEADER_FORMAT)
RECORD = struct.calcsize(RECORD_FORMAT)


def main() -> int:
    rom = memoryview(Path("cartridge.rom").read_bytes())

    (
        magic,
        count,
        stringsize,
        strings_compressed,
        trainsize,
        _slots,
        _seed,
        buckets_compressed,
        buffer,
    ) = struct.unpack_from(HEADER_FORMAT, rom, 0)

    if magic != MAGIC:
        print("not a cartridge.rom", file=sys.stderr)
        return 1

    cursor = HEADER
    records = rom[cursor : cursor + count * RECORD]
    cursor += count * RECORD
    cursor += buckets_compressed
    strings_blob = rom[cursor : cursor + strings_compressed]
    cursor += strings_compressed
    trained = rom[cursor : cursor + trainsize]

    strings = zstandard.ZstdDecompressor().decompress(
        strings_blob, max_output_size=stringsize
    )

    dictionary = zstandard.ZstdCompressionDict(trained)
    decoder = zstandard.ZstdDecompressor(dict_data=dictionary)

    root = Path("cartridge").resolve()

    for (
        position,
        compressed,
        uncompressed,
        offset,
        length,
        flags,
        algorithm,
    ) in struct.iter_unpack(RECORD_FORMAT, records):
        path = Path(strings[offset : offset + length].decode("utf-8"))
        if path.anchor or ".." in path.parts:
            raise ValueError("cartridge entry path must be relative")
        destination = (root / path).resolve()
        if not destination.is_relative_to(root):
            raise ValueError("cartridge entry path must be inside cartridge")

        if flags & DIRECTORY:
            destination.mkdir(parents=True, exist_ok=True)
            continue

        destination.parent.mkdir(parents=True, exist_ok=True)

        if uncompressed == 0:
            destination.write_bytes(b"")
            continue

        if algorithm == ALGO_RAW:
            destination.write_bytes(rom[position : position + uncompressed])
            continue

        if compressed > buffer:
            raise ValueError("compressed payload exceeds buffer")

        destination.write_bytes(
            decoder.decompress(
                rom[position : position + compressed], max_output_size=uncompressed
            )
        )

    print(f"extracted {count} entries to cartridge/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
