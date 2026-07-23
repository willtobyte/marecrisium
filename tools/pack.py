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
CAPACITY = 131072
LEVEL = 22
TEST_LEVEL = 9
EMPTY = 0xFFFFFFFF
PRIME = 0x9E3779B97F4A7C15
MASK64 = 0xFFFFFFFFFFFFFFFF
SEED_BUDGET = 4096


def prepare(data: bytes) -> tuple[tuple[int, ...], int, int]:
    n = len(data)
    head = n & ~7
    chunks = struct.unpack(f"<{head >> 3}Q", data[:head]) if head else ()
    tail = int.from_bytes(data[head:], "little") if head < n else 0
    return chunks, tail, n


def hashfn(prepared: tuple[tuple[int, ...], int, int], seed: int) -> int:
    chunks, tail, n = prepared
    h = seed ^ n
    for chunk in chunks:
        r = (h ^ chunk) * PRIME
        h = (r & MASK64) ^ (r >> 64)
    if n & 7:
        r = (h ^ tail) * PRIME
        h = (r & MASK64) ^ (r >> 64)
    return h


def build_perfect(
    prepared: list[tuple[tuple[int, ...], int, int]],
) -> tuple[int, int, list[int]]:
    count = len(prepared)
    if not count:
        return 0, 4, [EMPTY] * 4

    slots = 1 << max(2, (count * count // 4 - 1).bit_length())
    while True:
        mask = slots - 1
        buckets = [EMPTY] * slots
        touched: list[int] = []
        for seed in range(SEED_BUDGET):
            ok = True
            for index, p in enumerate(prepared):
                slot = hashfn(p, seed) & mask
                if buckets[slot] != EMPTY:
                    ok = False
                    break
                buckets[slot] = index
                touched.append(slot)
            if ok:
                return seed, slots, buckets
            for s in touched:
                buckets[s] = EMPTY
            touched.clear()
        slots *= 2


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

    prepared = [prepare(p) for p in encoded]
    seed, slots, buckets = build_perfect(prepared)

    plain = zstandard.ZstdCompressor(level=LEVEL, threads=-1)
    buckets = plain.compress(struct.pack(f"<{slots}I", *buckets))
    strings = plain.compress(bytes(strings))

    base = HEADER + count * RECORD + len(buckets) + len(strings) + trainsize

    blob = bytearray()
    blob.extend(
        struct.pack(
            "<IIIIIIIII",
            MAGIC,
            count,
            stringsize,
            len(strings),
            trainsize,
            slots,
            seed,
            len(buckets),
            0,
        )
    )

    cursor = 0
    for index, current in enumerate(sources):
        flags = DIRECTORY if current["directory"] else 0
        data_offset = 0 if current["directory"] else base + cursor
        blob.extend(
            struct.pack(
                "<IIIIHBB",
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

    blob.extend(buckets)
    blob.extend(strings)
    blob.extend(trained)

    for current in sources:
        blob.extend(current["blob"])

    Path("cartridge.rom").write_bytes(blob)
    print(f"created cartridge.rom ({count} entries, {len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
