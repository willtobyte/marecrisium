#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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

constexpr size_t CAPACITIES[] = {16 * 1024, 64 * 1024, 112 * 1024};
constexpr int LEVELS[] = {3, 9, 15, 19, 22};

struct entry final {
  std::string path;
  std::vector<uint8_t> data;
  std::vector<uint8_t> blob;
  bool directory{};
};

template <std::integral T>
void put(std::vector<uint8_t> &output, T value) {
  const auto offset = output.size();
  output.resize(offset + sizeof(T));
  std::memcpy(output.data() + offset, &value, sizeof(T));
}
}

int main(int argc, char **argv) {
  const std::filesystem::path root = argv[1];

  std::vector<entry> entries;
  for (const auto &it : std::filesystem::recursive_directory_iterator(root)) {
    auto relative = std::filesystem::relative(it.path(), root).generic_string();
    if (relative.empty() || relative == ".")
      continue;

    if (it.is_directory()) {
      entries.push_back({std::move(relative), {}, {}, true});
      continue;
    }

    if (!it.is_regular_file())
      continue;

    const auto size = static_cast<size_t>(std::filesystem::file_size(it.path()));
    std::vector<uint8_t> data(size);
    if (size > 0) {
      std::ifstream input(it.path(), std::ios::binary);
      input.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size));
    }
    entries.push_back({std::move(relative), std::move(data), {}, false});
  }

  std::ranges::sort(entries, {}, &entry::path);

  std::vector<uint8_t> training;
  std::vector<size_t> samples;
  for (const auto &e : entries) {
    if (e.directory || e.data.empty())
      continue;
    training.insert(training.end(), e.data.begin(), e.data.end());
    samples.push_back(e.data.size());
  }

  std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context(ZSTD_createCCtx(), ZSTD_freeCCtx);

  std::vector<uint8_t> dictionary;
  size_t best = SIZE_MAX;
  std::vector<std::vector<uint8_t>> winners;

  for (const auto capacity : CAPACITIES) {
    std::vector<uint8_t> candidate(capacity, 0);
    ZDICT_fastCover_params_t parameters = {};
    parameters.nbThreads = 1;
    parameters.zParams.compressionLevel = ZSTD_maxCLevel();

    const auto trained = ZDICT_optimizeTrainFromBuffer_fastCover(
      candidate.data(), candidate.size(),
      training.data(),
      samples.data(), static_cast<unsigned>(samples.size()),
      &parameters);
    if (ZDICT_isError(trained))
      continue;
    candidate.resize(trained);

    for (const auto level : LEVELS) {
      std::unique_ptr<ZSTD_CDict, decltype(&ZSTD_freeCDict)> compressor(
        ZSTD_createCDict(candidate.data(), candidate.size(), level),
        ZSTD_freeCDict);

      std::vector<std::vector<uint8_t>> blobs;
      blobs.reserve(entries.size());
      size_t total = candidate.size();
      std::vector<uint8_t> scratch;

      for (const auto &e : entries) {
        if (e.directory) {
          blobs.emplace_back();
          continue;
        }
        scratch.resize(ZSTD_compressBound(e.data.size()));
        const auto result = ZSTD_compress_usingCDict(
          context.get(), scratch.data(), scratch.size(),
          e.data.data(), e.data.size(), compressor.get());
        blobs.emplace_back(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(result));
        total += result;
      }

      if (total < best) {
        best = total;
        dictionary = candidate;
        winners = std::move(blobs);
      }
    }
  }

  uint64_t directories = 0;
  for (const auto &e : entries)
    directories += 2 + e.path.size() + METADATA;

  const auto footprint = static_cast<uint32_t>(dictionary.size());
  const uint64_t base = HEADER + footprint + directories;

  std::vector<uint8_t> blob;
  put(blob, MAGIC);
  put(blob, VERSION);
  put(blob, static_cast<uint32_t>(entries.size()));
  put(blob, directories);
  put(blob, footprint);
  blob.insert(blob.end(), dictionary.begin(), dictionary.end());

  uint64_t cursor = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto &e = entries[i];
    const auto &chunk = winners[i];
    put(blob, static_cast<uint16_t>(e.path.size()));
    blob.insert(blob.end(), e.path.begin(), e.path.end());
    const uint64_t address = e.directory ? 0 : base + cursor;
    put(blob, address);
    put(blob, static_cast<uint64_t>(chunk.size()));
    put(blob, static_cast<uint64_t>(e.data.size()));
    blob.push_back(e.directory ? DIRECTORY : 0);
    cursor += chunk.size();
  }

  for (const auto &chunk : winners)
    blob.insert(blob.end(), chunk.begin(), chunk.end());

  std::ofstream output("cartridge.rom", std::ios::binary);
  output.write(reinterpret_cast<const char *>(blob.data()), static_cast<std::streamsize>(blob.size()));

  std::cout << "created cartridge.rom (" << entries.size() << " entries, "
            << blob.size() << " bytes)\n";

  return 0;
}
