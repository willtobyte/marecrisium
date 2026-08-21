#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.14"
# dependencies = ["zstandard"]
# ///

import importlib
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path


def human_size(size: int) -> str:
    if size >= 1_048_576:
        return f"{size / 1_048_576:.1f} MiB"
    if size >= 1024:
        return f"{size / 1024:.1f} KiB"
    return f"{size} bytes"


def human_time(elapsed: float) -> str:
    if elapsed >= 60_000:
        return f"{elapsed / 60_000:.1f} min"
    if elapsed >= 1000:
        return f"{elapsed / 1000:.1f} s"
    return f"{elapsed:.0f} ms"


zstandard = importlib.import_module("zstandard")
ZstdError = zstandard.ZstdError

MAGIC = b"CROM"
DIRECTORY = 2
ALGO_RAW = 0
ALGO_ZSTD_DICT = 1
HEADER_FORMAT = "<4s5I40x"
RECORD_FORMAT = "<Q4I2B6x"
HEADER = struct.calcsize(HEADER_FORMAT)
RECORD = struct.calcsize(RECORD_FORMAT)
CAPACITY = 131072
LEVEL = 22
TEST_LEVEL = 9
EMPTY = 0xFFFF
PRIME = 0x9E3779B97F4A7C15
MASK64 = 0xFFFFFFFFFFFFFFFF


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


def build_table(digests: list[int]) -> tuple[int, list[int]]:
    count = len(digests)
    slots = 4
    while slots < count * 2:
        slots *= 2

    mask = slots - 1
    buckets = [EMPTY] * slots
    for index, value in enumerate(digests):
        slot = value & mask
        while buckets[slot] != EMPTY:
            slot = (slot + 1) & mask
        buckets[slot] = index
    return slots, buckets


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
    start = time.perf_counter()
    root = Path("cartridge")
    output = Path("cartridge.rom")
    output.unlink(missing_ok=True)

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

    samples: list[bytes] = []
    for current in sources:
        if current.directory:
            continue
        if current.data and len(probe.compress(current.data)) < len(current.data):
            samples.append(current.data)
        else:
            current.blob = current.data
            current.algorithm = ALGO_RAW

    try:
        dictionary = zstandard.train_dictionary(
            CAPACITY,
            samples,
            split_point=1.0,
            level=TEST_LEVEL,
            threads=-1,
        )
    except ZstdError:
        dictionary = zstandard.ZstdCompressionDict(b"\0")

    trained = dictionary.as_bytes()

    encoder = zstandard.ZstdCompressor(level=LEVEL, dict_data=dictionary, threads=-1)
    for current in sources:
        if current.directory or current.algorithm == ALGO_RAW:
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

    digests = [hashfn(prepare(p), 0) for p in encoded]

    seen: dict[int, str] = {}
    for value, current in zip(digests, sources):
        if value in seen:
            print(f"hash collision: {seen[value]} and {current.path}", file=sys.stderr)
            return 1
        seen[value] = current.path

    seed = 0
    slots, buckets = build_table(digests)

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
                digests[index],
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

    output.write_bytes(blob)

    display(sources)
    elapsed = (time.perf_counter() - start) * 1000
    print(
        f"cartridge.rom ({count} entries, {human_size(len(blob))})"
        f" in {human_time(elapsed)}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
