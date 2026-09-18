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

DIRECTORY = 2
ALGO_STORED = 0
HEADER_FORMAT = "<4x5I8x"
RECORD_FORMAT = "<Q4I2B6x"
HEADER = struct.calcsize(HEADER_FORMAT)
RECORD = struct.calcsize(RECORD_FORMAT)


def main() -> int:
    with Path("cartridge.rom").open("rb") as stream:
        count, stringsize, trainsize, slots, _ = struct.unpack_from(
            HEADER_FORMAT, stream.read(HEADER), 0
        )

        rom = memoryview(stream.read(slots * RECORD + stringsize + trainsize))
        cursor = 0
        records = rom[cursor : cursor + slots * RECORD]
        cursor += slots * RECORD
        strings = rom[cursor : cursor + stringsize]
        cursor += stringsize
        trained = rom[cursor : cursor + trainsize]

        dictionary = zstandard.ZstdCompressionDict(trained)
        decoder = zstandard.ZstdDecompressor(dict_data=dictionary)

        root = Path("cartridge").resolve()

        for (
            digest,
            position,
            compressed,
            uncompressed,
            offset,
            length,
            kind,
        ) in sorted(
            struct.iter_unpack(RECORD_FORMAT, records),
            key=lambda record: record[1],
        ):
            if digest == 0:
                continue

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

            stream.seek(position)
            if kind == ALGO_STORED:
                destination.write_bytes(stream.read(uncompressed))
                continue

            destination.write_bytes(
                decoder.decompress(
                    stream.read(compressed),
                    max_output_size=uncompressed,
                )
            )

    print(f"extracted {count} entries to cartridge/")

    return 0


if __name__ == "__main__":
    sys.exit(main())
