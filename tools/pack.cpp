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
constexpr uint32_t CROM_MAGIC = 0x43524F4D;
constexpr uint32_t CROM_VERSION = 1;
constexpr uint8_t FLAG_DIRECTORY = 1;
constexpr size_t HEADER_SIZE = 24;
constexpr size_t METADATA_SIZE = 25;

constexpr size_t DICTIONARY_CAPACITIES[] = {16 * 1024, 64 * 1024, 112 * 1024};
constexpr int COMPRESSION_LEVELS[] = {3, 9, 15, 19, 22};

struct entry final {
  std::string path;
  std::vector<uint8_t> data;
  std::vector<uint8_t> blob;
  bool directory{};
};

template <std::integral T>
void put(std::vector<uint8_t> &out, T value) {
  const auto offset = out.size();
  out.resize(offset + sizeof(T));
  std::memcpy(out.data() + offset, &value, sizeof(T));
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

  std::vector<uint8_t> training_buffer;
  std::vector<size_t> sample_sizes;
  for (const auto &e : entries) {
    if (e.directory || e.data.empty())
      continue;
    training_buffer.insert(training_buffer.end(), e.data.begin(), e.data.end());
    sample_sizes.push_back(e.data.size());
  }

  std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> cctx(ZSTD_createCCtx(), ZSTD_freeCCtx);

  std::vector<uint8_t> dictionary;
  size_t best_total = SIZE_MAX;
  std::vector<std::vector<uint8_t>> best_blobs;

  for (const auto dict_capacity : DICTIONARY_CAPACITIES) {
    std::vector<uint8_t> candidate_dict(dict_capacity, 0);
    ZDICT_fastCover_params_t params = {};
    params.nbThreads = 1;
    params.zParams.compressionLevel = ZSTD_maxCLevel();

    const auto trained = ZDICT_optimizeTrainFromBuffer_fastCover(
      candidate_dict.data(), candidate_dict.size(),
      training_buffer.data(),
      sample_sizes.data(), static_cast<unsigned>(sample_sizes.size()),
      &params);
    if (ZDICT_isError(trained))
      continue;
    candidate_dict.resize(trained);

    for (const auto level : COMPRESSION_LEVELS) {
      std::unique_ptr<ZSTD_CDict, decltype(&ZSTD_freeCDict)> cdict(
        ZSTD_createCDict(candidate_dict.data(), candidate_dict.size(), level),
        ZSTD_freeCDict);

      std::vector<std::vector<uint8_t>> blobs;
      blobs.reserve(entries.size());
      size_t total = candidate_dict.size();
      std::vector<uint8_t> scratch;

      for (const auto &e : entries) {
        if (e.directory) {
          blobs.emplace_back();
          continue;
        }
        scratch.resize(ZSTD_compressBound(e.data.size()));
        const auto result = ZSTD_compress_usingCDict(
          cctx.get(), scratch.data(), scratch.size(),
          e.data.data(), e.data.size(), cdict.get());
        blobs.emplace_back(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(result));
        total += result;
      }

      if (total < best_total) {
        best_total = total;
        dictionary = candidate_dict;
        best_blobs = std::move(blobs);
      }
    }
  }

  uint64_t directory_bytes = 0;
  for (const auto &e : entries)
    directory_bytes += 2 + e.path.size() + METADATA_SIZE;

  const auto dictionary_bytes = static_cast<uint32_t>(dictionary.size());
  const uint64_t data_base = HEADER_SIZE + dictionary_bytes + directory_bytes;

  std::vector<uint8_t> output;
  put(output, CROM_MAGIC);
  put(output, CROM_VERSION);
  put(output, static_cast<uint32_t>(entries.size()));
  put(output, directory_bytes);
  put(output, dictionary_bytes);
  output.insert(output.end(), dictionary.begin(), dictionary.end());

  uint64_t blob_offset = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto &e = entries[i];
    const auto &blob = best_blobs[i];
    put(output, static_cast<uint16_t>(e.path.size()));
    output.insert(output.end(), e.path.begin(), e.path.end());
    const uint64_t absolute_offset = e.directory ? 0 : data_base + blob_offset;
    put(output, absolute_offset);
    put(output, static_cast<uint64_t>(blob.size()));
    put(output, static_cast<uint64_t>(e.data.size()));
    output.push_back(e.directory ? FLAG_DIRECTORY : 0);
    blob_offset += blob.size();
  }

  for (const auto &blob : best_blobs)
    output.insert(output.end(), blob.begin(), blob.end());

  std::ofstream out("cartridge.rom", std::ios::binary);
  out.write(reinterpret_cast<const char *>(output.data()), static_cast<std::streamsize>(output.size()));

  std::cout << "created cartridge.rom (" << entries.size() << " entries, "
            << output.size() << " bytes)\n";

  return 0;
}
