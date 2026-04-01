#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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
  std::vector<uint8_t> data;
};

void write_u16(std::ostream &output, uint16_t value) {
  uint8_t buffer[2] = {
    static_cast<uint8_t>(value & 0xFF),
    static_cast<uint8_t>((value >> 8) & 0xFF),
  };

  output.write(reinterpret_cast<const char *>(buffer), 2);
}

void write_u32(std::ostream &output, uint32_t value) {
  uint8_t buffer[4] = {
    static_cast<uint8_t>(value & 0xFF),
    static_cast<uint8_t>((value >> 8) & 0xFF),
    static_cast<uint8_t>((value >> 16) & 0xFF),
    static_cast<uint8_t>((value >> 24) & 0xFF),
  };

  output.write(reinterpret_cast<const char *>(buffer), 4);
}

void write_u64(std::ostream &output, uint64_t value) {
  uint8_t buffer[8];
  for (int i = 0; i < 8; ++i)
    buffer[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
  output.write(reinterpret_cast<const char *>(buffer), 8);
}
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: pack <directory>\n";
    return 1;
  }

  const std::filesystem::path root = argv[1];
  if (!std::filesystem::is_directory(root)) {
    std::cerr << "error: " << root << " is not a directory\n";
    return 1;
  }

  std::vector<entry> entries;

  std::vector<std::filesystem::path> paths;
  for (const auto &it : std::filesystem::recursive_directory_iterator(root))
    paths.push_back(it.path());
  std::sort(paths.begin(), paths.end());

  for (const auto &path : paths) {
    auto relative = std::filesystem::relative(path, root).generic_string();
    if (relative == "." || relative.empty())
      continue;

    entry e;
    e.path = std::move(relative);
    e.offset = 0;
    e.compressed = 0;
    e.uncompressed = 0;
    e.flags = 0;

    if (std::filesystem::is_directory(path)) {
      e.flags = FLAG_DIRECTORY;
    } else if (std::filesystem::is_regular_file(path)) {
      std::ifstream input(path, std::ios::binary);
      if (!input) {
        std::cerr << "error: cannot read " << path << "\n";
        return 1;
      }

      const auto size = std::filesystem::file_size(path);
      std::vector<uint8_t> raw(static_cast<size_t>(size));
      input.read(reinterpret_cast<char *>(raw.data()), static_cast<std::streamsize>(size));

      e.uncompressed = size;

      const auto bound = ZSTD_compressBound(raw.size());
      e.data.resize(bound);

      const auto result = ZSTD_compress(
        e.data.data(), e.data.size(),
        raw.data(), raw.size(), ZSTD_defaultCLevel());

      if (ZSTD_isError(result)) {
        std::cerr << "error: zstd compression failed for " << path << ": " << ZSTD_getErrorName(result) << "\n";
        return 1;
      }

      e.data.resize(result);
      e.compressed = result;
    } else {
      continue;
    }

    entries.push_back(std::move(e));
  }

  uint64_t toc_size = 0;
  for (const auto &e : entries)
    toc_size += 2 + e.path.size() + 8 + 8 + 8 + 1;

  uint64_t data_offset = 12 + toc_size;
  for (auto &e : entries) {
    if (!(e.flags & FLAG_DIRECTORY)) {
      e.offset = data_offset;
      data_offset += e.compressed;
    }
  }

  std::ofstream output("cartridge.rom", std::ios::binary);
  if (!output) {
    std::cerr << "error: cannot create cartridge.rom\n";
    return 1;
  }

  write_u32(output, CROM_MAGIC);
  write_u32(output, CROM_VERSION);
  write_u32(output, static_cast<uint32_t>(entries.size()));

  for (const auto &e : entries) {
    write_u16(output, static_cast<uint16_t>(e.path.size()));
    output.write(e.path.data(), static_cast<std::streamsize>(e.path.size()));
    write_u64(output, e.offset);
    write_u64(output, e.compressed);
    write_u64(output, e.uncompressed);
    output.put(static_cast<char>(e.flags));
  }

  for (const auto &e : entries) {
    if (!(e.flags & FLAG_DIRECTORY) && !e.data.empty())
      output.write(reinterpret_cast<const char *>(e.data.data()),
                static_cast<std::streamsize>(e.data.size()));
  }

  const auto total = output.tellp();
  std::cout << "created cartridge.rom (" << entries.size() << " entries, " << total << " bytes)\n";

  return 0;
}
