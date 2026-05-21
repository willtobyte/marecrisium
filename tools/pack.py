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
HEADER = 16
RECORD = 32
CAPACITY = 131072
LEVEL = 22
TEST_LEVEL = 9


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: pack <dir>", file=sys.stderr)
        return 1

    root = Path(sys.argv[1])

    sources: list[dict] = []
    for current in root.rglob("*"):
        relative = current.relative_to(root)
        if any(part.startswith(".") for part in relative.parts):
            continue

        path = relative.as_posix()
        if not path or path == ".":
            continue

        if current.is_dir():
            sources.append(
                {
                    "path": path,
                    "data": b"",
                    "blob": b"",
                    "directory": True,
                    "algorithm": ALGO_RAW,
                }
            )
            continue

        if not current.is_file():
            continue

        data = current.read_bytes()
        sources.append(
            {
                "path": path,
                "data": data,
                "blob": b"",
                "directory": False,
                "algorithm": ALGO_ZSTD_DICT,
            }
        )

    sources.sort(key=lambda current: current["path"])

    probe = zstandard.ZstdCompressor(level=TEST_LEVEL, threads=-1)

    samples = [
        current["data"]
        for current in sources
        if not current["directory"]
        and current["data"]
        and len(probe.compress(current["data"])) < len(current["data"])
    ]

    dictionary = zstandard.train_dictionary(
        CAPACITY,
        samples,
        split_point=1.0,
        level=LEVEL,
        threads=-1,
    )
    trained = dictionary.as_bytes()

    encoder = zstandard.ZstdCompressor(level=LEVEL, dict_data=dictionary, threads=-1)
    for current in sources:
        if current["directory"]:
            continue
        compressed = encoder.compress(current["data"])
        if len(compressed) < len(current["data"]):
            current["blob"] = compressed
        else:
            current["blob"] = current["data"]
            current["algorithm"] = ALGO_RAW

    strings = bytearray()
    offsets: list[int] = []
    encoded: list[bytes] = []
    for current in sources:
        bytes_ = current["path"].encode("utf-8")
        encoded.append(bytes_)
        offsets.append(len(strings))
        strings.extend(bytes_)

    count = len(sources)
    stringsize = len(strings)
    trainsize = len(trained)
    base = HEADER + count * RECORD + stringsize + trainsize

    blob = bytearray()
    blob.extend(struct.pack("<IIII", MAGIC, count, stringsize, trainsize))

    cursor = 0
    for index, current in enumerate(sources):
        flags = DIRECTORY if current["directory"] else 0
        data_offset = 0 if current["directory"] else base + cursor
        blob.extend(
            struct.pack(
                "<QQQIHBB",
                data_offset,
                len(current["blob"]),
                len(current["data"]),
                offsets[index],
                len(encoded[index]),
                flags,
                current["algorithm"],
            )
        )
        cursor += len(current["blob"])

    blob.extend(strings)
    blob.extend(trained)

    for current in sources:
        blob.extend(current["blob"])

    Path("cartridge.rom").write_bytes(blob)
    print(f"created cartridge.rom ({count} entries, {len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
