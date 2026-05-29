#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.13"
# dependencies = ["zstandard"]
# ///

import struct
import sys
from pathlib import Path

import zstandard

MAGIC = 0x4D4F5243
DIRECTORY = 1
ALGO_RAW = 0
ALGO_ZSTD_DICT = 1
HEADER = 36
RECORD = 20


def main() -> int:
    rom = Path("cartridge.rom").read_bytes()

    (
        magic,
        count,
        stringsize,
        strings_compressed,
        trainsize,
        _slots,
        _seed,
        buckets_compressed,
        _reserved,
    ) = struct.unpack_from("<IIIIIIIII", rom, 0)

    if magic != MAGIC:
        print("not a cartridge.rom", file=sys.stderr)
        return 1

    cursor = HEADER
    records = rom[cursor : cursor + count * RECORD]
    cursor += count * RECORD
    cursor += buckets_compressed
    strings_blob = rom[cursor : cursor + strings_compressed]
    cursor += strings_compressed
    trained = bytes(rom[cursor : cursor + trainsize])

    strings = zstandard.ZstdDecompressor().decompress(
        strings_blob, max_output_size=stringsize
    )

    dictionary = zstandard.ZstdCompressionDict(trained)
    decoder = zstandard.ZstdDecompressor(dict_data=dictionary)

    root = Path("cartridge")

    for index in range(count):
        (
            position,
            compressed,
            uncompressed,
            offset,
            length,
            flags,
            algorithm,
        ) = struct.unpack_from("<IIIIHBB", records, index * RECORD)

        path = strings[offset : offset + length].decode("utf-8")
        destination = root / path

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

        payload = bytes(rom[position : position + compressed])
        destination.write_bytes(
            decoder.decompress(payload, max_output_size=uncompressed)
        )

    print(f"extracted {count} entries to cartridge/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
