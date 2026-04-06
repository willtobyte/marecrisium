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
  uint64_t offset{};
  uint64_t compressed{};
  uint64_t uncompressed{};
  uint8_t flags{};
};

template <typename T>
T read_le(const uint8_t *p) noexcept {
  T v = 0;
  for (size_t i = 0; i < sizeof(T); ++i)
    v |= static_cast<T>(p[i]) << (i * 8);
  return v;
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

  if (read_le<uint32_t>(header) != CROM_MAGIC) {
    std::cerr << "error: invalid magic\n";
    return 1;
  }

  const auto version = read_le<uint32_t>(header + 4);
  if (version != CROM_VERSION) {
    std::cerr << "error: unsupported version " << version << "\n";
    return 1;
  }

  const auto count = read_le<uint32_t>(header + 8);
  std::vector<entry> entries;
  entries.reserve(count);

  for (uint32_t i = 0; i < count; ++i) {
    uint8_t length[2];
    input.read(reinterpret_cast<char *>(length), 2);
    if (!input) {
      std::cerr << "error: cannot read path length\n";
      return 1;
    }

    std::string path(read_le<uint16_t>(length), '\0');
    input.read(path.data(), static_cast<std::streamsize>(path.size()));
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
      read_le<uint64_t>(metadata),
      read_le<uint64_t>(metadata + 8),
      read_le<uint64_t>(metadata + 16),
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
        decompressed.data(),
        decompressed.size(),
        compressed.data(),
        compressed.size());

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
