#!/usr/bin/env python3
"""Generate seed corpus files for the CROM archive fuzzer.

Usage: python3 corpus.py <output_dir>

CROM binary format (little-endian):
  Header (24 bytes): magic(u32) + version(u32) + count(u32) + directory_bytes(u64) + dictionary_bytes(u32)
  Dictionary (dictionary_bytes): raw zstd dictionary
  TOC entries (variable, total = directory_bytes): length(u16) + path(bytes) + offset(u64) + compressed(u64) + uncompressed(u64) + flags(u8)
  File data: zstd-compressed blobs at the offsets declared in the TOC
"""

import os
import struct
import sys

import zstandard

CROM_MAGIC = 0x43524F4D
CROM_VERSION = 1
FLAG_DIRECTORY = 1
HEADER_SIZE = 24


def header(
    magic=CROM_MAGIC,
    version=CROM_VERSION,
    count=0,
    directory_bytes=0,
    dictionary_bytes=0,
):
    return struct.pack(
        "<IIIQI", magic, version, count, directory_bytes, dictionary_bytes
    )


def record(path, offset=0, compressed=0, uncompressed=0, flags=0):
    encoded = path.encode("utf-8") if isinstance(path, str) else path
    return (
        struct.pack("<H", len(encoded))
        + encoded
        + struct.pack("<QQQB", offset, compressed, uncompressed, flags)
    )


def train_dictionary():
    # Train a tiny dictionary from synthetic samples resembling game assets.
    samples = (
        [
            b"level = {width=%d, height=%d, enemies=%d}" % (w, h, e)
            for w in (100, 200, 400, 800)
            for h in (60, 120, 240)
            for e in (0, 3, 7, 15)
        ]
        + [
            b'{"name":"sprite_%03d","frames":[0,1,2,3],"loop":true}' % i
            for i in range(32)
        ]
        + [
            b"function update_%d(dt) self.x = self.x + %d * dt end" % (i, i)
            for i in range(16)
        ]
    )
    return zstandard.train_dictionary(4096, samples).as_bytes()


DICTIONARY = train_dictionary()
_compressor = zstandard.ZstdCompressor(
    dict_data=zstandard.ZstdCompressionDict(DICTIONARY)
)


def compress(data):
    return _compressor.compress(data)


def toc_size(entries):
    total = 0
    for path, _, _, _, _ in entries:
        encoded = path.encode("utf-8") if isinstance(path, str) else path
        total += 2 + len(encoded) + 25
    return total


def build_rom(entries, file_contents=None, dictionary=DICTIONARY):
    """Build a valid ROM.

    entries: list of (path, offset, compressed, uncompressed, flags) tuples.
      offset/compressed/uncompressed may be None to be computed.
    file_contents: maps entry index to raw (already compressed) data blob.
    dictionary: embedded dictionary bytes (defaults to the shared training dict).
    """
    if file_contents is None:
        file_contents = {}

    computed = []
    ts = toc_size(entries)
    data_offset = HEADER_SIZE + len(dictionary) + ts

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

    rom = header(
        count=len(entries),
        directory_bytes=ts,
        dictionary_bytes=len(dictionary),
    )
    rom += dictionary
    for path, offset, compressed_size, uncompressed_size, flags, _ in computed:
        rom += record(path, offset, compressed_size, uncompressed_size, flags)
    for _, _, _, _, _, blob in computed:
        rom += blob
    return rom


def generate(output):
    os.makedirs(output, exist_ok=True)
    seeds = {}

    # Empty ROM (0 entries) — still carries the dictionary.
    seeds["valid_empty"] = build_rom([])

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

    # Header-only ROM (no dict, no TOC, no data) — invalid: dictionary_bytes=0.
    seeds["boundary_exact_header"] = header()

    # Path with length=0
    seeds["boundary_empty_path"] = build_rom([("", None, None, None, 0)])

    # Long path (1000+ bytes)
    long_path = "a" * 1200
    seeds["boundary_long_path"] = build_rom([(long_path, None, None, None, 0)])

    # Path with null bytes
    null_path = b"test\x00file.txt"
    seeds["boundary_null_in_path"] = (
        header(
            count=1,
            directory_bytes=2 + len(null_path) + 25,
            dictionary_bytes=len(DICTIONARY),
        )
        + DICTIONARY
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
    seeds["size_offset_zero"] = (
        header(
            count=1,
            directory_bytes=toc_size([("file.txt", 0, 0, 0, 0)]),
            dictionary_bytes=len(DICTIONARY),
        )
        + DICTIONARY
        + record("file.txt", offset=0, compressed=10, uncompressed=10, flags=0)
    )

    # offset pointing to exact end of file
    rom = build_rom([("eof.bin", None, None, 5, 0)], {0: compress(b"hello")})
    eof_offset = len(rom)
    seeds["size_offset_at_eof"] = (
        header(
            count=1,
            directory_bytes=toc_size([("eof.bin", 0, 0, 0, 0)]),
            dictionary_bytes=len(DICTIONARY),
        )
        + DICTIONARY
        + record("eof.bin", offset=eof_offset, compressed=10, uncompressed=10, flags=0)
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
    shared_offset = HEADER_SIZE + len(DICTIONARY) + ts
    rom = (
        header(count=2, directory_bytes=ts, dictionary_bytes=len(DICTIONARY))
        + DICTIONARY
    )
    rom += record(
        "file_a.txt",
        offset=shared_offset,
        compressed=len(blob),
        uncompressed=len(raw),
        flags=0,
    )
    rom += record(
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

    # Truncated headers (0 through HEADER_SIZE-1 bytes)
    full_header = header(dictionary_bytes=len(DICTIONARY))
    for length in range(HEADER_SIZE):
        seeds[f"corrupt_truncated_{length:02d}"] = full_header[:length]

    # TOC entry truncated at various points
    full_toc = (
        header(
            count=1,
            directory_bytes=toc_size([("test.txt", 0, 0, 0, 0)]),
            dictionary_bytes=len(DICTIONARY),
        )
        + DICTIONARY
        + record("test.txt", 0, 0, 0, 0)
    )
    for trim in [1, 5, 10, 15, 20, 25]:
        seeds[f"corrupt_toc_truncated_{trim:02d}"] = full_toc[:-trim]

    # Count larger than actual entries
    seeds["corrupt_count_larger"] = (
        header(
            count=100,
            directory_bytes=toc_size([("only.txt", 0, 0, 0, 0)]),
            dictionary_bytes=len(DICTIONARY),
        )
        + DICTIONARY
        + record("only.txt", 0, 0, 0, 0)
    )

    # Count = max uint32
    seeds["corrupt_count_max"] = header(count=0xFFFFFFFF)

    # Count = max uint32 followed by some data
    seeds["corrupt_count_max_data"] = header(count=0xFFFFFFFF) + b"\x00" * 100

    # Dictionary-specific adversarial cases.
    seeds["dict_missing"] = header(count=0, directory_bytes=0, dictionary_bytes=0)
    seeds["dict_truncated"] = (
        header(
            count=0,
            directory_bytes=0,
            dictionary_bytes=len(DICTIONARY),
        )
        + DICTIONARY[: len(DICTIONARY) // 2]
    )
    seeds["dict_declared_huge"] = header(
        count=0, directory_bytes=0, dictionary_bytes=0xFFFFFFFF
    )
    seeds["dict_garbage"] = header(
        count=0, directory_bytes=0, dictionary_bytes=128
    ) + bytes(range(128))
    seeds["dict_zeros"] = (
        header(count=0, directory_bytes=0, dictionary_bytes=128) + b"\x00" * 128
    )

    # Valid zstd data
    raw = b"the quick brown fox jumps over the lazy dog"
    blob = compress(raw)
    seeds["zstd_valid"] = build_rom(
        [("zstd_ok.txt", None, None, len(raw), 0)],
        {0: blob},
    )

    # Random bytes where zstd data should be
    ts = toc_size([("random.bin", 0, 0, 0, 0)])
    data_offset = HEADER_SIZE + len(DICTIONARY) + ts
    garbage = bytes(range(256))[:64]
    rom = (
        header(count=1, directory_bytes=ts, dictionary_bytes=len(DICTIONARY))
        + DICTIONARY
        + record(
            "random.bin",
            offset=data_offset,
            compressed=len(garbage),
            uncompressed=100,
            flags=0,
        )
        + garbage
    )
    seeds["zstd_random_bytes"] = rom

    # Truncated zstd frame
    blob = compress(b"this is a longer string for compression testing purposes" * 10)
    truncated = blob[: len(blob) // 2]
    ts = toc_size([("truncated.bin", 0, 0, 0, 0)])
    data_offset = HEADER_SIZE + len(DICTIONARY) + ts
    rom = (
        header(count=1, directory_bytes=ts, dictionary_bytes=len(DICTIONARY))
        + DICTIONARY
        + record(
            "truncated.bin",
            offset=data_offset,
            compressed=len(truncated),
            uncompressed=560,
            flags=0,
        )
        + truncated
    )
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
