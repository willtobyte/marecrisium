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
constexpr uint8_t UNCOMPRESSED = 2;
constexpr size_t HEADER = 16;
constexpr size_t RECORD = 32;

using decoder_t = std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)>;

using dictionary_t = std::unique_ptr<ZSTD_DDict, decltype(&ZSTD_freeDDict)>;

struct record final {
  uint64_t data_offset;
  uint64_t compressed;
  uint64_t uncompressed;
  uint32_t path_offset;
  uint16_t path_length;
  uint8_t flags;
  uint8_t padding;
};

static_assert(sizeof(record) == RECORD);

template <std::integral Integer>
Integer read(const uint8_t *pointer) noexcept {
  Integer value;
  std::memcpy(&value, pointer, sizeof(Integer));
  return value;
}
}

int main() {
  std::ifstream input("cartridge.rom", std::ios::binary | std::ios::ate);
  const auto size = static_cast<size_t>(input.tellg());
  input.seekg(0);
  std::vector<uint8_t> rom(size);
  input.read(reinterpret_cast<char *>(rom.data()), static_cast<std::streamsize>(size));

  const auto count = read<uint32_t>(rom.data() + 4);
  const auto stringsize = read<uint32_t>(rom.data() + 8);
  const auto trainsize = read<uint32_t>(rom.data() + 12);

  const auto *trained = rom.data() + HEADER;
  const auto *strings = trained + trainsize;
  const auto *records = strings + stringsize;

  decoder_t decoder(ZSTD_createDCtx(), ZSTD_freeDCtx);
  dictionary_t dictionary(ZSTD_createDDict(trained, trainsize), ZSTD_freeDDict);

  const std::filesystem::path root = "cartridge";
  std::vector<uint8_t> decompressed;

  for (uint32_t index = 0; index < count; ++index) {
    record entry;
    std::memcpy(&entry, records + index * RECORD, RECORD);

    const std::string_view path{
      reinterpret_cast<const char *>(strings + entry.path_offset),
      entry.path_length
    };

    const auto destination = root / path;

    if (entry.flags & DIRECTORY) {
      std::filesystem::create_directories(destination);
      continue;
    }

    std::filesystem::create_directories(destination.parent_path());

    std::ofstream output(destination, std::ios::binary);
    if (entry.uncompressed == 0)
      continue;

    if ((entry.flags & UNCOMPRESSED) != 0) {
      output.write(
        reinterpret_cast<const char *>(rom.data() + entry.data_offset),
        static_cast<std::streamsize>(entry.uncompressed)
      );
      continue;
    }

    if (decompressed.size() < entry.uncompressed)
      decompressed.resize(entry.uncompressed);

    ZSTD_decompress_usingDDict(
      decoder.get(),
      decompressed.data(), entry.uncompressed,
      rom.data() + entry.data_offset, entry.compressed,
      dictionary.get()
    );

    output.write(
      reinterpret_cast<const char *>(decompressed.data()),
      static_cast<std::streamsize>(entry.uncompressed)
    );
  }

  std::cout << "extracted " << count << " entries to cartridge/\n";

  return 0;
}
