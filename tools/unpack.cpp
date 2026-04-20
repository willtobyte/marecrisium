#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <zstd.h>

namespace {
constexpr uint32_t CROM_MAGIC = 0x43524F4D;
constexpr uint32_t CROM_VERSION = 1;
constexpr uint8_t FLAG_DIRECTORY = 1;
constexpr size_t HEADER_SIZE = 24;
constexpr size_t METADATA_SIZE = 25;

template <std::integral T>
T read(const uint8_t *p) {
  T v;
  std::memcpy(&v, p, sizeof(T));
  return v;
}
}

int main() {
  std::ifstream input("cartridge.rom", std::ios::binary | std::ios::ate);
  if (!input) {
    std::cerr << "error: cannot open cartridge.rom\n";
    return 1;
  }

  const auto file_size = static_cast<size_t>(input.tellg());
  input.seekg(0);
  std::vector<uint8_t> rom(file_size);
  if (file_size && !input.read(reinterpret_cast<char *>(rom.data()),
                               static_cast<std::streamsize>(file_size))) {
    std::cerr << "error: cannot read cartridge.rom\n";
    return 1;
  }

  if (rom.size() < HEADER_SIZE) {
    std::cerr << "error: file too small for header\n";
    return 1;
  }

  if (read<uint32_t>(rom.data()) != CROM_MAGIC) {
    std::cerr << "error: invalid magic\n";
    return 1;
  }

  const auto version = read<uint32_t>(rom.data() + 4);
  if (version != CROM_VERSION) {
    std::cerr << "error: unsupported version " << version << "\n";
    return 1;
  }

  const auto count = read<uint32_t>(rom.data() + 8);
  const auto directory_bytes = read<uint64_t>(rom.data() + 12);
  const auto dictionary_bytes = read<uint32_t>(rom.data() + 20);

  if (dictionary_bytes == 0) {
    std::cerr << "error: missing dictionary\n";
    return 1;
  }

  if (HEADER_SIZE + dictionary_bytes + directory_bytes > rom.size()) {
    std::cerr << "error: truncated header region\n";
    return 1;
  }

  const auto *dictionary_data = rom.data() + HEADER_SIZE;

  std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> dctx(ZSTD_createDCtx(), ZSTD_freeDCtx);
  if (!dctx) {
    std::cerr << "error: ZSTD_createDCtx failed\n";
    return 1;
  }

  std::unique_ptr<ZSTD_DDict, decltype(&ZSTD_freeDDict)> ddict(
    ZSTD_createDDict(dictionary_data, dictionary_bytes), ZSTD_freeDDict);
  if (!ddict) {
    std::cerr << "error: ZSTD_createDDict failed\n";
    return 1;
  }

  const std::filesystem::path root = "cartridge";
  const auto *cursor = rom.data() + HEADER_SIZE + dictionary_bytes;
  const auto *end = cursor + directory_bytes;
  std::unique_ptr<uint8_t[]> decompressed;
  size_t decompressed_capacity = 0;

  for (uint32_t i = 0; i < count; ++i) {
    if (static_cast<size_t>(end - cursor) < 2u) {
      std::cerr << "error: truncated directory at entry " << i << "\n";
      return 1;
    }

    const auto length = read<uint16_t>(cursor);
    cursor += 2;

    if (static_cast<size_t>(end - cursor) < static_cast<size_t>(length) + METADATA_SIZE) {
      std::cerr << "error: truncated directory at entry " << i << "\n";
      return 1;
    }

    const std::string_view path{reinterpret_cast<const char *>(cursor), length};
    const auto *metadata = cursor + length;
    cursor += static_cast<size_t>(length) + METADATA_SIZE;

    const auto offset = read<uint64_t>(metadata);
    const auto compressed = read<uint64_t>(metadata + 8);
    const auto uncompressed = read<uint64_t>(metadata + 16);
    const auto flags = metadata[24];

    const auto destination = root / path;

    if (flags & FLAG_DIRECTORY) {
      std::filesystem::create_directories(destination);
      continue;
    }

    std::filesystem::create_directories(destination.parent_path());

    if (offset + compressed > rom.size()) {
      std::cerr << "error: blob out of bounds for " << path << "\n";
      return 1;
    }

    if (uncompressed > decompressed_capacity) {
      decompressed = std::make_unique_for_overwrite<uint8_t[]>(static_cast<size_t>(uncompressed));
      decompressed_capacity = static_cast<size_t>(uncompressed);
    }

    if (uncompressed > 0) {
      const auto result = ZSTD_decompress_usingDDict(
        dctx.get(),
        decompressed.get(), static_cast<size_t>(uncompressed),
        rom.data() + offset, static_cast<size_t>(compressed),
        ddict.get());

      if (ZSTD_isError(result) || result != uncompressed) {
        std::cerr << "error: decompression failed for " << path << "\n";
        return 1;
      }
    }

    std::ofstream output(destination, std::ios::binary);
    if (!output) {
      std::cerr << "error: cannot create " << destination << "\n";
      return 1;
    }

    if (uncompressed > 0)
      output.write(reinterpret_cast<const char *>(decompressed.get()),
                   static_cast<std::streamsize>(uncompressed));
  }

  std::cout << "extracted " << count << " entries to cartridge/\n";

  return 0;
}
