#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <zstd.h>

#define ZDICT_STATIC_LINKING_ONLY
#include <zdict.h>

namespace {
constexpr uint32_t MAGIC = 0x43524F4D;
constexpr uint8_t DIRECTORY = 1;
constexpr size_t HEADER = 16;
constexpr size_t RECORD = 32;
constexpr size_t CAPACITY = 2048;
constexpr int LEVEL = 22;

using encoder_t = std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)>;

using dictionary_t = std::unique_ptr<ZSTD_CDict, decltype(&ZSTD_freeCDict)>;

struct source final {
  std::string path;
  std::vector<uint8_t> data;
  std::vector<uint8_t> blob;
  bool directory{};
};

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
void put(std::vector<uint8_t> &output, Integer value) {
  const auto offset = output.size();
  output.resize(offset + sizeof(Integer));
  std::memcpy(output.data() + offset, &value, sizeof(Integer));
}

void put(std::vector<uint8_t> &output, std::span<const uint8_t> bytes) {
  output.insert(output.end(), bytes.begin(), bytes.end());
}
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::fputs("usage: pack <dir>\n", stderr);
    return 1;
  }

  const std::filesystem::path root = argv[1];

  std::vector<source> sources;
  for (const auto &it : std::filesystem::recursive_directory_iterator(root)) {
    auto relative = std::filesystem::relative(it.path(), root).generic_string();
    if (relative.empty() || relative == ".")
      continue;

    if (it.is_directory()) {
      sources.emplace_back(std::move(relative), std::vector<uint8_t>{}, std::vector<uint8_t>{}, true);
      continue;
    }

    if (!it.is_regular_file())
      continue;

    const auto size = static_cast<size_t>(std::filesystem::file_size(it.path()));
    std::vector<uint8_t> data(size);
    std::ifstream input(it.path(), std::ios::binary);
    input.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size));
    sources.emplace_back(std::move(relative), std::move(data), std::vector<uint8_t>{}, false);
  }

  std::ranges::sort(sources, {}, &source::path);

  std::vector<uint8_t> training;
  std::vector<size_t> samples;
  for (const auto &current : sources) {
    if (current.directory || current.data.empty() || !current.path.ends_with(".lua"))
      continue;
    training.insert(training.end(), current.data.begin(), current.data.end());
    samples.push_back(current.data.size());
  }

  std::vector<uint8_t> trained(CAPACITY, 0);
  ZDICT_fastCover_params_t parameters = {};
  parameters.nbThreads = std::thread::hardware_concurrency();
  parameters.splitPoint = 1.0;
  parameters.zParams.compressionLevel = LEVEL;
  const auto trained_size = ZDICT_optimizeTrainFromBuffer_fastCover(
    trained.data(), trained.size(),
    training.data(), samples.data(), static_cast<unsigned>(samples.size()),
    &parameters);
  trained.resize(trained_size);

  encoder_t encoder(ZSTD_createCCtx(), ZSTD_freeCCtx);
  dictionary_t dictionary(
    ZSTD_createCDict(trained.data(), trained.size(), LEVEL),
    ZSTD_freeCDict);
  ZSTD_CCtx_setParameter(encoder.get(), ZSTD_c_nbWorkers, std::thread::hardware_concurrency());
  ZSTD_CCtx_refCDict(encoder.get(), dictionary.get());

  std::vector<uint8_t> scratch;
  for (auto &current : sources) {
    if (current.directory)
      continue;
    scratch.resize(ZSTD_compressBound(current.data.size()));
    const auto result = ZSTD_compress2(
      encoder.get(), scratch.data(), scratch.size(),
      current.data.data(), current.data.size());
    current.blob.assign(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(result));
  }

  std::vector<uint8_t> strings;
  std::vector<uint32_t> string_offsets;
  string_offsets.reserve(sources.size());
  for (const auto &current : sources) {
    string_offsets.push_back(static_cast<uint32_t>(strings.size()));
    strings.insert(strings.end(), current.path.begin(), current.path.end());
  }

  const auto count = static_cast<uint32_t>(sources.size());
  const auto string_table_bytes = static_cast<uint32_t>(strings.size());
  const auto dictionary_bytes = static_cast<uint32_t>(trained.size());
  const uint64_t payload_base = HEADER + dictionary_bytes + string_table_bytes + static_cast<uint64_t>(count) * RECORD;

  std::vector<uint8_t> blob;
  blob.reserve(payload_base);

  put(blob, MAGIC);
  put(blob, count);
  put(blob, string_table_bytes);
  put(blob, dictionary_bytes);
  put(blob, std::span<const uint8_t>(trained));
  put(blob, std::span<const uint8_t>(strings));

  uint64_t cursor = 0;
  for (uint32_t index = 0; index < count; ++index) {
    const auto &current = sources[index];
    record entry{
      current.directory ? 0 : payload_base + cursor,
      static_cast<uint64_t>(current.blob.size()),
      static_cast<uint64_t>(current.data.size()),
      string_offsets[index],
      static_cast<uint16_t>(current.path.size()),
      static_cast<uint8_t>(current.directory ? DIRECTORY : 0),
      0,
    };
    const auto offset = blob.size();
    blob.resize(offset + RECORD);
    std::memcpy(blob.data() + offset, &entry, RECORD);
    cursor += current.blob.size();
  }

  for (const auto &current : sources)
    put(blob, std::span<const uint8_t>(current.blob));

  std::ofstream output("cartridge.rom", std::ios::binary);
  output.write(reinterpret_cast<const char *>(blob.data()), static_cast<std::streamsize>(blob.size()));

  std::cout << std::format("created cartridge.rom ({} entries, {} bytes)\n", sources.size(), blob.size());

  return 0;
}
