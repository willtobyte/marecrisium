#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.13"
# dependencies = ["zstandard"]
# ///

import importlib
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

zstandard = importlib.import_module("zstandard")
ZstdError = zstandard.ZstdError

MAGIC = b"CRO2"
DIRECTORY = 2
ALGO_RAW = 0
ALGO_ZSTD_DICT = 1
HEADER_FORMAT = "<4s5I40x"
RECORD_FORMAT = "<3IH2B"
HEADER = struct.calcsize(HEADER_FORMAT)
RECORD = struct.calcsize(RECORD_FORMAT)
CAPACITY = 131072
LEVEL = 22
TEST_LEVEL = 9
EMPTY = 0xFFFF
PRIME = 0x9E3779B97F4A7C15
MASK64 = 0xFFFFFFFFFFFFFFFF
SEED_BUDGET = 4096


@dataclass(slots=True)
class Source:
    path: str
    data: bytes
    directory: bool
    blob: bytes = b""
    algorithm: int = ALGO_RAW


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

    target = max(32, (count * count + 15) // 16)
    slots = 1 << (target - 1).bit_length()
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


def display(
    sources: list[Source], index: int = 0, parent: str = "", prefix: str = ""
) -> None:
    count = len(sources)
    while index < count:
        path = sources[index].path
        folder, _, name = path.rpartition("/")
        if folder != parent:
            break

        end = index + 1
        stem = f"{path}/"
        while end < count and sources[end].path.startswith(stem):
            end += 1

        last = end == count or sources[end].path.rpartition("/")[0] != parent
        print(f"{prefix}{'`--' if last else '|--'} {name}")
        if end > index + 1:
            display(sources, index + 1, path, prefix + ("    " if last else "|   "))

        index = end


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: pack <dir>", file=sys.stderr)
        return 1

    root = Path(sys.argv[1])

    sources: list[Source] = []
    for current in root.rglob("*"):
        relative = current.relative_to(root)
        if any(part.startswith(".") for part in relative.parts):
            continue

        path = relative.as_posix()
        if current.is_dir():
            sources.append(Source(path, b"", True))
        elif current.is_file():
            sources.append(
                Source(path, current.read_bytes(), False, algorithm=ALGO_ZSTD_DICT)
            )

    sources.sort(key=lambda current: current.path)

    probe = zstandard.ZstdCompressor(level=TEST_LEVEL, threads=-1)

    samples = [
        current.data
        for current in sources
        if not current.directory
        and current.data
        and len(probe.compress(current.data)) < len(current.data)
    ]

    try:
        dictionary = zstandard.train_dictionary(
            CAPACITY,
            samples,
            split_point=1.0,
            level=LEVEL,
            threads=-1,
        )
    except ZstdError:
        dictionary = zstandard.ZstdCompressionDict(b"\0")

    trained = dictionary.as_bytes()

    encoder = zstandard.ZstdCompressor(level=LEVEL, dict_data=dictionary, threads=-1)
    for current in sources:
        if current.directory:
            continue
        compressed = encoder.compress(current.data)
        if len(compressed) < len(current.data):
            current.blob = compressed
        else:
            current.blob = current.data
            current.algorithm = ALGO_RAW

    strings = bytearray()
    offsets: list[int] = []
    encoded: list[bytes] = []
    for current in sources:
        name = current.path.encode("utf-8")
        encoded.append(name)
        offsets.append(len(strings))
        strings.extend(name)

    count = len(sources)
    stringsize = len(strings)
    trainsize = len(trained)

    prepared = [prepare(p) for p in encoded]
    seed, slots, buckets = build_perfect(prepared)

    buckets = struct.pack(f"<{slots}H", *buckets)

    base = HEADER + len(buckets) + count * RECORD + stringsize + trainsize

    blob = bytearray(
        struct.pack(
            HEADER_FORMAT,
            MAGIC,
            count,
            stringsize,
            trainsize,
            slots,
            seed,
        )
    )
    blob.extend(buckets)

    cursor = 0
    for index, current in enumerate(sources):
        kind = DIRECTORY if current.directory else current.algorithm
        data_offset = 0 if current.directory else base + cursor
        blob.extend(
            struct.pack(
                RECORD_FORMAT,
                data_offset,
                len(current.blob),
                len(current.data),
                offsets[index],
                len(encoded[index]),
                kind,
            )
        )
        cursor += len(current.blob)

    blob.extend(strings)
    blob.extend(trained)

    for current in sources:
        blob.extend(current.blob)

    Path("cartridge.rom").write_bytes(blob)

    display(sources)
    print(f"created cartridge.rom ({count} entries, {len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
