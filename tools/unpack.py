#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.13"
# dependencies = [
#   "zstandard>=0.25.0",
# ]
# ///

import struct
import sys
from pathlib import Path

import zstandard

DIRECTORY = 1
UNCOMPRESSED = 2
HEADER = 16
RECORD = 32


def main() -> int:
    rom = Path("cartridge.rom").read_bytes()

    _, count, stringsize, trainsize = struct.unpack_from("<IIII", rom, 0)

    cursor = HEADER
    trained = bytes(rom[cursor : cursor + trainsize])
    cursor += trainsize
    strings = rom[cursor : cursor + stringsize]
    cursor += stringsize
    records = rom[cursor : cursor + count * RECORD]

    dictionary = zstandard.ZstdCompressionDict(trained)
    decoder = zstandard.ZstdDecompressor(dict_data=dictionary)

    root = Path("cartridge")

    for index in range(count):
        offset = index * RECORD
        data_offset, compressed, uncompressed, path_offset, path_length, flags, _ = (
            struct.unpack_from("<QQQIHBB", records, offset)
        )

        path = strings[path_offset : path_offset + path_length].decode("utf-8")
        destination = root / path

        if flags & DIRECTORY:
            destination.mkdir(parents=True, exist_ok=True)
            continue

        destination.parent.mkdir(parents=True, exist_ok=True)

        if uncompressed == 0:
            destination.write_bytes(b"")
            continue

        if flags & UNCOMPRESSED:
            destination.write_bytes(rom[data_offset : data_offset + uncompressed])
            continue

        payload = bytes(rom[data_offset : data_offset + compressed])
        destination.write_bytes(
            decoder.decompress(payload, max_output_size=uncompressed)
        )

    print(f"extracted {count} entries to cartridge/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
