#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <zstd.h>

namespace {
constexpr uint32_t CROM_MAGIC = 0x43524F4D;
constexpr uint32_t CROM_VERSION = 1;
constexpr uint8_t FLAG_DIRECTORY = 1;

struct entry {
  std::string path;
  uint64_t offset;
  uint64_t compressed;
  uint64_t uncompressed;
  uint8_t flags;
};

uint16_t read_u16(const uint8_t *pointer) noexcept {
  return static_cast<uint16_t>(
    static_cast<uint16_t>(pointer[0]) |
    static_cast<uint16_t>(static_cast<uint16_t>(pointer[1]) << 8));
}

uint32_t read_u32(const uint8_t *pointer) noexcept {
  return static_cast<uint32_t>(pointer[0]) |
         (static_cast<uint32_t>(pointer[1]) << 8) |
         (static_cast<uint32_t>(pointer[2]) << 16) |
         (static_cast<uint32_t>(pointer[3]) << 24);
}

uint64_t read_u64(const uint8_t *pointer) noexcept {
  return static_cast<uint64_t>(pointer[0]) |
         (static_cast<uint64_t>(pointer[1]) << 8) |
         (static_cast<uint64_t>(pointer[2]) << 16) |
         (static_cast<uint64_t>(pointer[3]) << 24) |
         (static_cast<uint64_t>(pointer[4]) << 32) |
         (static_cast<uint64_t>(pointer[5]) << 40) |
         (static_cast<uint64_t>(pointer[6]) << 48) |
         (static_cast<uint64_t>(pointer[7]) << 56);
}
}

int main() {
  std::ifstream input("cartridge.rom", std::ios::binary);
  if (!input) {
    std::cerr << "error: cannot open cartridge.rom\n";
    return 1;
  }

  uint8_t header[12];
  input.read(reinterpret_cast<char *>(header), sizeof(header));
  if (!input) {
    std::cerr << "error: cannot read header\n";
    return 1;
  }

  const auto magic = read_u32(header);
  if (magic != CROM_MAGIC) {
    std::cerr << "error: invalid magic\n";
    return 1;
  }

  const auto version = read_u32(header + 4);
  if (version != CROM_VERSION) {
    std::cerr << "error: unsupported version " << version << "\n";
    return 1;
  }

  const auto entry_count = read_u32(header + 8);
  std::vector<entry> entries;
  entries.reserve(entry_count);

  for (uint32_t i = 0; i < entry_count; ++i) {
    uint8_t length_bytes[2];
    input.read(reinterpret_cast<char *>(length_bytes), 2);
    if (!input) {
      std::cerr << "error: cannot read path length\n";
      return 1;
    }

    const auto path_length = read_u16(length_bytes);
    std::string path(path_length, '\0');
    input.read(path.data(), path_length);
    if (!input) {
      std::cerr << "error: cannot read path\n";
      return 1;
    }

    uint8_t metadata[25];
    input.read(reinterpret_cast<char *>(metadata), sizeof(metadata));
    if (!input) {
      std::cerr << "error: cannot read entry metadata\n";
      return 1;
    }

    entries.push_back({
      std::move(path),
      read_u64(metadata),
      read_u64(metadata + 8),
      read_u64(metadata + 16),
      metadata[24],
    });
  }

  const std::filesystem::path root = "cartridge";

  for (const auto &entry : entries) {
    const auto destination = root / entry.path;

    if (entry.flags & FLAG_DIRECTORY) {
      std::filesystem::create_directories(destination);
      continue;
    }

    std::filesystem::create_directories(destination.parent_path());

    std::vector<uint8_t> compressed(static_cast<size_t>(entry.compressed));
    input.seekg(static_cast<std::streamoff>(entry.offset));
    input.read(reinterpret_cast<char *>(compressed.data()),
               static_cast<std::streamsize>(entry.compressed));
    if (!input) {
      std::cerr << "error: cannot read data for " << entry.path << "\n";
      return 1;
    }

    std::vector<uint8_t> decompressed(static_cast<size_t>(entry.uncompressed));

    if (entry.uncompressed > 0) {
      const auto result = ZSTD_decompress(
        decompressed.data(), decompressed.size(),
        compressed.data(), compressed.size());

      if (ZSTD_isError(result)) {
        std::cerr << "error: decompression failed for " << entry.path << ": " << ZSTD_getErrorName(result) << "\n";
        return 1;
      }
    }

    std::ofstream output(destination, std::ios::binary);
    if (!output) {
      std::cerr << "error: cannot create " << destination << "\n";
      return 1;
    }

    output.write(reinterpret_cast<const char *>(decompressed.data()),
                 static_cast<std::streamsize>(decompressed.size()));
  }

  std::cout << "extracted " << entries.size() << " entries to cartridge/\n";

  return 0;
}
