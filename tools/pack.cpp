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
constexpr uint32_t VERSION = 1;
constexpr uint8_t DIRECTORY = 1;
constexpr size_t HEADER = 24;
constexpr size_t METADATA = 25;
constexpr size_t CAPACITY = 2048;
constexpr int LEVEL = 22;

using context = std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)>;

using compressor = std::unique_ptr<ZSTD_CDict, decltype(&ZSTD_freeCDict)>;

struct record final {
  std::string path;
  std::vector<uint8_t> data;
  std::vector<uint8_t> blob;
  bool directory{};
};

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

  std::vector<record> records;
  for (const auto &it : std::filesystem::recursive_directory_iterator(root)) {
    auto relative = std::filesystem::relative(it.path(), root).generic_string();
    if (relative.empty() || relative == ".")
      continue;

    if (it.is_directory()) {
      records.emplace_back(std::move(relative), std::vector<uint8_t>{}, std::vector<uint8_t>{}, true);
      continue;
    }

    if (!it.is_regular_file())
      continue;

    const auto size = static_cast<size_t>(std::filesystem::file_size(it.path()));
    std::vector<uint8_t> data(size);
    std::ifstream input(it.path(), std::ios::binary);
    input.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size));
    records.emplace_back(std::move(relative), std::move(data), std::vector<uint8_t>{}, false);
  }

  std::ranges::sort(records, {}, &record::path);

  std::vector<uint8_t> training;
  std::vector<size_t> samples;
  for (const auto &current : records) {
    if (current.directory || current.data.empty() || !current.path.ends_with(".lua"))
      continue;
    training.insert(training.end(), current.data.begin(), current.data.end());
    samples.push_back(current.data.size());
  }

  std::vector<uint8_t> dictionary(CAPACITY, 0);
  ZDICT_fastCover_params_t parameters = {};
  parameters.nbThreads = std::thread::hardware_concurrency();
  parameters.splitPoint = 1.0;
  parameters.zParams.compressionLevel = LEVEL;
  const auto trained = ZDICT_optimizeTrainFromBuffer_fastCover(
    dictionary.data(), dictionary.size(),
    training.data(), samples.data(), static_cast<unsigned>(samples.size()),
    &parameters);
  dictionary.resize(trained);

  context engine(ZSTD_createCCtx(), ZSTD_freeCCtx);
  compressor bundle(
    ZSTD_createCDict(dictionary.data(), dictionary.size(), LEVEL),
    ZSTD_freeCDict);
  ZSTD_CCtx_refCDict(engine.get(), bundle.get());
  ZSTD_CCtx_setParameter(engine.get(), ZSTD_c_nbWorkers, std::thread::hardware_concurrency());

  std::vector<uint8_t> scratch;
  for (auto &current : records) {
    if (current.directory)
      continue;
    scratch.resize(ZSTD_compressBound(current.data.size()));
    const auto result = ZSTD_compress2(
      engine.get(), scratch.data(), scratch.size(),
      current.data.data(), current.data.size());
    current.blob.assign(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(result));
  }

  uint64_t directories = 0;
  for (const auto &current : records)
    directories += 2 + current.path.size() + METADATA;

  const auto bytes = static_cast<uint32_t>(dictionary.size());
  const uint64_t base = HEADER + bytes + directories;

  std::vector<uint8_t> blob;
  put(blob, MAGIC);
  put(blob, VERSION);
  put(blob, static_cast<uint32_t>(records.size()));
  put(blob, directories);
  put(blob, bytes);
  put(blob, std::span<const uint8_t>(dictionary));

  uint64_t cursor = 0;
  for (const auto &current : records) {
    put(blob, static_cast<uint16_t>(current.path.size()));
    put(blob, std::span<const uint8_t>(reinterpret_cast<const uint8_t *>(current.path.data()), current.path.size()));
    const uint64_t address = current.directory ? 0 : base + cursor;
    put(blob, address);
    put(blob, static_cast<uint64_t>(current.blob.size()));
    put(blob, static_cast<uint64_t>(current.data.size()));
    uint8_t flags = 0;
    if (current.directory) flags |= DIRECTORY;
    blob.push_back(flags);
    cursor += current.blob.size();
  }

  for (const auto &current : records)
    put(blob, std::span<const uint8_t>(current.blob));

  std::ofstream output("cartridge.rom", std::ios::binary);
  output.write(reinterpret_cast<const char *>(blob.data()), static_cast<std::streamsize>(blob.size()));

  std::cout << std::format("created cartridge.rom ({} entries, {} bytes)\n", records.size(), blob.size());

  return 0;
}
