#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

#include <zstd.h>

namespace {
constexpr uint8_t DIRECTORY = 1;
constexpr size_t HEADER = 24;
constexpr size_t METADATA = 25;

template <std::integral T>
T read(const uint8_t *p) {
  T v;
  std::memcpy(&v, p, sizeof(T));
  return v;
}
}

int main() {
  std::ifstream input("cartridge.rom", std::ios::binary | std::ios::ate);
  const auto size = static_cast<size_t>(input.tellg());
  input.seekg(0);
  std::vector<uint8_t> rom(size);
  input.read(reinterpret_cast<char *>(rom.data()), static_cast<std::streamsize>(size));

  const auto count = read<uint32_t>(rom.data() + 8);
  const auto directories = read<uint64_t>(rom.data() + 12);
  const auto footprint = read<uint32_t>(rom.data() + 20);

  std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> context(ZSTD_createDCtx(), ZSTD_freeDCtx);
  std::unique_ptr<ZSTD_DDict, decltype(&ZSTD_freeDDict)> decompressor(
    ZSTD_createDDict(rom.data() + HEADER, footprint), ZSTD_freeDDict);

  const std::filesystem::path root = "cartridge";
  const auto *cursor = rom.data() + HEADER + footprint;
  std::vector<uint8_t> decompressed;

  for (uint32_t i = 0; i < count; ++i) {
    const auto length = read<uint16_t>(cursor);
    cursor += 2;

    const std::string_view path{reinterpret_cast<const char *>(cursor), length};
    const auto *metadata = cursor + length;
    cursor += static_cast<size_t>(length) + METADATA;

    const auto offset = read<uint64_t>(metadata);
    const auto compressed = read<uint64_t>(metadata + 8);
    const auto uncompressed = read<uint64_t>(metadata + 16);
    const auto flags = metadata[24];

    const auto destination = root / path;

    if (flags & DIRECTORY) {
      std::filesystem::create_directories(destination);
      continue;
    }

    std::filesystem::create_directories(destination.parent_path());

    if (uncompressed > decompressed.size())
      decompressed.resize(static_cast<size_t>(uncompressed));

    if (uncompressed > 0)
      ZSTD_decompress_usingDDict(
        context.get(),
        decompressed.data(), static_cast<size_t>(uncompressed),
        rom.data() + offset, static_cast<size_t>(compressed),
        decompressor.get());

    std::ofstream output(destination, std::ios::binary);
    if (uncompressed > 0)
      output.write(reinterpret_cast<const char *>(decompressed.data()),
                   static_cast<std::streamsize>(uncompressed));
  }

  std::cout << "extracted " << count << " entries to cartridge/\n";

  return 0;
}
