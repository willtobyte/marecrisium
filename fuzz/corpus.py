#!/usr/bin/env python3
"""Generate seed corpus files for the CROM archive fuzzer.

Usage: python3 corpus.py <output_dir>

CROM binary format (little-endian):
  Header (12 bytes): magic(u32) + version(u32) + count(u32)
  TOC entries (variable): length(u16) + path(bytes) + offset(u64) + compressed(u64) + uncompressed(u64) + flags(u8)
  File data: zstd-compressed blobs at the offsets declared in the TOC
"""

import os
import struct
import sys

import zstandard

CROM_MAGIC = 0x43524F4D
CROM_VERSION = 1
FLAG_DIRECTORY = 1


def header(magic=CROM_MAGIC, version=CROM_VERSION, count=0):
    return struct.pack("<III", magic, version, count)


def toc_entry(path, offset=0, compressed=0, uncompressed=0, flags=0):
    encoded = path.encode("utf-8") if isinstance(path, str) else path
    return (
        struct.pack("<H", len(encoded))
        + encoded
        + struct.pack("<QQQB", offset, compressed, uncompressed, flags)
    )


def compress(data):
    compressor = zstandard.ZstdCompressor()
    return compressor.compress(data)


def toc_size(entries):
    total = 0
    for path, _, _, _, _ in entries:
        encoded = path.encode("utf-8") if isinstance(path, str) else path
        total += 2 + len(encoded) + 25
    return total


def build_rom(entries, file_contents=None):
    """Build a valid ROM from a list of (path, offset, compressed, uncompressed, flags) tuples.

    If file_contents is provided, it maps entry indices to raw (already compressed) data blobs.
    When offset/compressed/uncompressed are None, they are computed automatically.
    """
    if file_contents is None:
        file_contents = {}

    computed = []
    ts = toc_size(entries)
    data_offset = 12 + ts

    for index, (path, offset, compressed_size, uncompressed_size, flags) in enumerate(
        entries
    ):
        blob = file_contents.get(index, b"")
        if offset is None:
            offset = data_offset
        if compressed_size is None:
            compressed_size = len(blob)
        if uncompressed_size is None:
            uncompressed_size = 0
        computed.append((path, offset, compressed_size, uncompressed_size, flags, blob))
        data_offset += len(blob)

    rom = header(count=len(entries))
    for path, offset, compressed_size, uncompressed_size, flags, _ in computed:
        rom += toc_entry(path, offset, compressed_size, uncompressed_size, flags)
    for _, _, _, _, _, blob in computed:
        rom += blob
    return rom


def generate(output):
    os.makedirs(output, exist_ok=True)
    seeds = {}

    # Empty ROM (0 entries)
    seeds["valid_empty"] = header()

    # Single directory entry
    seeds["valid_one_dir"] = build_rom([("scripts", None, None, None, FLAG_DIRECTORY)])

    # Single file entry
    raw = b"hello world"
    blob = compress(raw)
    seeds["valid_one_file"] = build_rom(
        [("test.txt", None, None, len(raw), 0)],
        {0: blob},
    )

    # Multiple entries (dirs + files)
    raw_a = b"content of file a"
    raw_b = b"content of file b, slightly longer data here"
    blob_a = compress(raw_a)
    blob_b = compress(raw_b)
    seeds["valid_multiple"] = build_rom(
        [
            ("scripts", None, None, None, FLAG_DIRECTORY),
            ("blobs", None, None, None, FLAG_DIRECTORY),
            ("scripts/main.lua", None, None, len(raw_a), 0),
            ("blobs/sprite.png", None, None, len(raw_b), 0),
        ],
        {2: blob_a, 3: blob_b},
    )

    # Nested directories with multiple files
    files = {
        "data/levels/level1.lua": b"level = {width=100}",
        "data/levels/level2.lua": b"level = {width=200, height=150}",
        "data/sprites/player.png": b"\x89PNG" + b"\x00" * 50,
        "data/sounds/jump.opus": b"OggS" + b"\x00" * 30,
    }
    entries = []
    contents = {}
    dirs = set()
    for path in sorted(files.keys()):
        parts = path.split("/")
        for depth in range(1, len(parts)):
            directory = "/".join(parts[:depth])
            if directory not in dirs:
                dirs.add(directory)
                entries.append((directory, None, None, None, FLAG_DIRECTORY))
    for path in sorted(files.keys()):
        raw = files[path]
        blob = compress(raw)
        index = len(entries)
        entries.append((path, None, None, len(raw), 0))
        contents[index] = blob
    seeds["valid_nested"] = build_rom(entries, contents)

    # Various file sizes
    for size in [0, 1, 255, 256, 1024, 4096, 65535]:
        raw = bytes(range(256)) * ((size // 256) + 1)
        raw = raw[:size]
        blob = compress(raw)
        seeds[f"valid_size_{size}"] = build_rom(
            [(f"file_{size}.bin", None, None, len(raw), 0)],
            {0: blob},
        )

    # Exact 12 byte header (no TOC, no data)
    seeds["boundary_exact_header"] = header()

    # Path with length=0
    seeds["boundary_empty_path"] = build_rom([("", None, None, None, 0)])

    # Long path (1000+ bytes)
    long_path = "a" * 1200
    seeds["boundary_long_path"] = build_rom([(long_path, None, None, None, 0)])

    # Path with null bytes
    seeds["boundary_null_in_path"] = build_rom([])
    null_path = b"test\x00file.txt"
    seeds["boundary_null_in_path"] = (
        header(count=1)
        + struct.pack("<H", len(null_path))
        + null_path
        + struct.pack("<QQQB", 0, 0, 0, 0)
    )

    # Path with unicode
    seeds["boundary_unicode_path"] = build_rom(
        [("\u00e9\u00e0\u00fc\u4e16\u754c.txt", None, None, None, 0)]
    )

    # Path with only slashes
    seeds["boundary_only_slashes"] = build_rom([("///", None, None, None, 0)])

    # Path without directory separator
    seeds["boundary_no_slash"] = build_rom([("flatfile", None, None, None, 0)])

    # Many entries
    many_entries = [(f"entry_{i:04d}.txt", None, None, None, 0) for i in range(200)]
    seeds["boundary_many_entries"] = build_rom(many_entries)

    # compressed=0 and uncompressed=0
    seeds["size_zero_both"] = build_rom([("empty.bin", None, None, 0, 0)])

    # uncompressed much larger than compressed
    big_raw = b"\x00" * 100000
    big_blob = compress(big_raw)
    seeds["size_large_uncompressed"] = build_rom(
        [("big.bin", None, None, len(big_raw), 0)],
        {0: big_blob},
    )

    # offset=0 (pointing into header)
    seeds["size_offset_zero"] = header(count=1) + toc_entry(
        "file.txt", offset=0, compressed=10, uncompressed=10, flags=0
    )

    # offset pointing to exact end of file
    rom = build_rom([("eof.bin", None, None, 5, 0)], {0: compress(b"hello")})
    eof_offset = len(rom)
    seeds["size_offset_at_eof"] = header(count=1) + toc_entry(
        "eof.bin", offset=eof_offset, compressed=10, uncompressed=10, flags=0
    )

    # Overlapping offsets
    raw = b"shared data payload"
    blob = compress(raw)
    ts = toc_size(
        [
            ("file_a.txt", 0, 0, 0, 0),
            ("file_b.txt", 0, 0, 0, 0),
        ]
    )
    shared_offset = 12 + ts
    rom = header(count=2)
    rom += toc_entry(
        "file_a.txt",
        offset=shared_offset,
        compressed=len(blob),
        uncompressed=len(raw),
        flags=0,
    )
    rom += toc_entry(
        "file_b.txt",
        offset=shared_offset,
        compressed=len(blob),
        uncompressed=len(raw),
        flags=0,
    )
    rom += blob
    seeds["size_overlapping_offsets"] = rom

    # Declared uncompressed size mismatched (larger than actual)
    raw = b"small"
    blob = compress(raw)
    seeds["size_mismatch_larger"] = build_rom(
        [("mismatch.bin", None, None, 99999, 0)],
        {0: blob},
    )

    # Declared uncompressed size mismatched (smaller than actual)
    seeds["size_mismatch_smaller"] = build_rom(
        [("mismatch2.bin", None, None, 1, 0)],
        {0: blob},
    )

    # Wrong magic (zero)
    seeds["corrupt_magic_zero"] = header(magic=0x00000000)

    # Wrong magic (all ones)
    seeds["corrupt_magic_ones"] = header(magic=0xFFFFFFFF)

    # Almost correct magic (off by one)
    seeds["corrupt_magic_near"] = header(magic=0x43524F4E)

    # Wrong version (0)
    seeds["corrupt_version_zero"] = header(version=0)

    # Wrong version (2)
    seeds["corrupt_version_two"] = header(version=2)

    # Wrong version (max)
    seeds["corrupt_version_max"] = header(version=0xFFFFFFFF)

    # Truncated headers (0 through 11 bytes)
    full_header = header()
    for length in range(12):
        seeds[f"corrupt_truncated_{length:02d}"] = full_header[:length]

    # TOC entry truncated at various points
    full_toc = header(count=1) + toc_entry("test.txt", 0, 0, 0, 0)
    for trim in [1, 5, 10, 15, 20, 25]:
        seeds[f"corrupt_toc_truncated_{trim:02d}"] = full_toc[:-trim]

    # Count larger than actual entries
    seeds["corrupt_count_larger"] = header(count=100) + toc_entry(
        "only.txt", 0, 0, 0, 0
    )

    # Count = max uint32
    seeds["corrupt_count_max"] = header(count=0xFFFFFFFF)

    # Count = max uint32 followed by some data
    seeds["corrupt_count_max_data"] = header(count=0xFFFFFFFF) + b"\x00" * 100

    # Valid zstd data
    raw = b"the quick brown fox jumps over the lazy dog"
    blob = compress(raw)
    seeds["zstd_valid"] = build_rom(
        [("zstd_ok.txt", None, None, len(raw), 0)],
        {0: blob},
    )

    # Random bytes where zstd data should be
    ts = toc_size([("random.bin", 0, 0, 0, 0)])
    data_offset = 12 + ts
    garbage = bytes(range(256))[:64]
    rom = header(count=1)
    rom += toc_entry(
        "random.bin",
        offset=data_offset,
        compressed=len(garbage),
        uncompressed=100,
        flags=0,
    )
    rom += garbage
    seeds["zstd_random_bytes"] = rom

    # Truncated zstd frame
    blob = compress(b"this is a longer string for compression testing purposes" * 10)
    truncated = blob[: len(blob) // 2]
    ts = toc_size([("truncated.bin", 0, 0, 0, 0)])
    data_offset = 12 + ts
    rom = header(count=1)
    rom += toc_entry(
        "truncated.bin",
        offset=data_offset,
        compressed=len(truncated),
        uncompressed=560,
        flags=0,
    )
    rom += truncated
    seeds["zstd_truncated"] = rom

    # Empty zstd frame (compressed size = 0, uncompressed > 0)
    seeds["zstd_empty_frame"] = build_rom([("empty_zstd.bin", None, None, 100, 0)])

    # Zstd decompresses to different size than declared
    raw = b"exact size test data"
    blob = compress(raw)
    seeds["zstd_wrong_declared_size"] = build_rom(
        [("wrong_size.bin", None, None, len(raw) * 3, 0)],
        {0: blob},
    )

    # Valid zstd but declared uncompressed=0
    seeds["zstd_declared_zero"] = build_rom(
        [("zero_declared.bin", None, None, 0, 0)],
        {0: blob},
    )

    # FLAG_DIRECTORY on a file that has data
    raw = b"directory with data"
    blob = compress(raw)
    seeds["flags_dir_with_data"] = build_rom(
        [("fake_dir", None, None, len(raw), FLAG_DIRECTORY)],
        {0: blob},
    )

    # Unknown flags (0xFF)
    seeds["flags_all_set"] = build_rom([("weird_flags.txt", None, None, None, 0xFF)])

    # Zero flags on what should be a directory (no FLAG_DIRECTORY)
    seeds["flags_zero_on_dir"] = build_rom([("looks/like/dir", None, None, None, 0)])

    # Flag = 0xFE (everything except directory)
    seeds["flags_non_dir_bits"] = build_rom([("flagged.bin", None, None, None, 0xFE)])

    for name, data in sorted(seeds.items()):
        path = os.path.join(output, name + ".rom")
        with open(path, "wb") as handle:
            handle.write(data)

    print(f"generated {len(seeds)} corpus files in {output}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <output_dir>", file=sys.stderr)
        sys.exit(1)

    generate(sys.argv[1])
