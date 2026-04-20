#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <zdict.h>
#include <zstd.h>

namespace {
constexpr uint32_t CROM_MAGIC = 0x43524F4D;
constexpr uint32_t CROM_VERSION = 2;
constexpr uint8_t FLAG_DIRECTORY = 1;
constexpr size_t HEADER_SIZE = 24;
constexpr size_t METADATA_SIZE = 25;
constexpr size_t DICTIONARY_CAPACITY = 112 * 1024;
constexpr size_t DICTIONARY_MIN_SAMPLES = 7;

struct entry final {
  std::string path;
  uint64_t blob_offset{};
  uint64_t blob_size{};
  uint64_t uncompressed{};
  uint8_t flags{};
};

template <std::integral T>
void put(std::vector<uint8_t> &out, T value) {
  uint8_t buffer[sizeof(T)];
  std::memcpy(buffer, &value, sizeof(T));
  out.insert(out.end(), buffer, buffer + sizeof(T));
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

  std::vector<std::filesystem::path> paths;
  for (const auto &it : std::filesystem::recursive_directory_iterator(root))
    paths.push_back(it.path());
  std::ranges::sort(paths);

  struct sample final {
    std::string relative;
    std::vector<uint8_t> data;
    bool directory;
  };

  std::vector<sample> samples;
  samples.reserve(paths.size());

  for (const auto &path : paths) {
    auto relative = std::filesystem::relative(path, root).generic_string();
    if (relative == "." || relative.empty())
      continue;

    if (std::filesystem::is_directory(path)) {
      samples.push_back({std::move(relative), {}, true});
      continue;
    }

    if (!std::filesystem::is_regular_file(path))
      continue;

    std::ifstream input(path, std::ios::binary);
    if (!input) {
      std::cerr << "error: cannot read " << path << "\n";
      return 1;
    }

    const auto size = static_cast<size_t>(std::filesystem::file_size(path));
    std::vector<uint8_t> data(size);
    if (size > 0)
      input.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size));

    samples.push_back({std::move(relative), std::move(data), false});
  }

  // Train dictionary from file samples (non-directory entries).
  std::vector<uint8_t> dictionary;
  std::vector<uint8_t> training_buffer;
  std::vector<size_t> sample_sizes;
  for (const auto &s : samples) {
    if (s.directory || s.data.empty())
      continue;
    training_buffer.insert(training_buffer.end(), s.data.begin(), s.data.end());
    sample_sizes.push_back(s.data.size());
  }

  if (sample_sizes.size() >= DICTIONARY_MIN_SAMPLES) {
    dictionary.resize(DICTIONARY_CAPACITY);
    const auto trained = ZDICT_trainFromBuffer(
      dictionary.data(), dictionary.size(),
      training_buffer.data(),
      sample_sizes.data(), static_cast<unsigned>(sample_sizes.size()));

    if (ZDICT_isError(trained)) {
      std::cerr << "warning: dictionary training failed: " << ZDICT_getErrorName(trained) << ", falling back to dictless\n";
      dictionary.clear();
    } else {
      dictionary.resize(trained);
    }
  }

  training_buffer.clear();
  training_buffer.shrink_to_fit();

  std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> cctx(ZSTD_createCCtx(), ZSTD_freeCCtx);
  if (!cctx) {
    std::cerr << "error: ZSTD_createCCtx failed\n";
    return 1;
  }

  std::unique_ptr<ZSTD_CDict, decltype(&ZSTD_freeCDict)> cdict(nullptr, ZSTD_freeCDict);
  if (!dictionary.empty()) {
    cdict.reset(ZSTD_createCDict(dictionary.data(), dictionary.size(), ZSTD_defaultCLevel()));
    if (!cdict) {
      std::cerr << "error: ZSTD_createCDict failed\n";
      return 1;
    }
  }

  std::vector<entry> entries;
  entries.reserve(samples.size());
  std::vector<uint8_t> blobs;
  std::vector<uint8_t> scratch;

  for (auto &s : samples) {
    if (s.directory) {
      entries.push_back({std::move(s.relative), 0, 0, 0, FLAG_DIRECTORY});
      continue;
    }

    const auto size = s.data.size();
    const auto bound = ZSTD_compressBound(size);
    scratch.resize(bound);

    const auto result = cdict
      ? ZSTD_compress_usingCDict(
          cctx.get(),
          scratch.data(), scratch.size(),
          s.data.data(), size,
          cdict.get())
      : ZSTD_compressCCtx(
          cctx.get(),
          scratch.data(), scratch.size(),
          s.data.data(), size,
          ZSTD_defaultCLevel());

    if (ZSTD_isError(result)) {
      std::cerr << "error: zstd compression failed for " << s.relative << ": " << ZSTD_getErrorName(result) << "\n";
      return 1;
    }

    entries.push_back({
      std::move(s.relative),
      static_cast<uint64_t>(blobs.size()),
      static_cast<uint64_t>(result),
      static_cast<uint64_t>(size),
      0,
    });
    blobs.insert(blobs.end(), scratch.data(), scratch.data() + result);
  }

  uint64_t directory_bytes = 0;
  for (const auto &e : entries)
    directory_bytes += 2 + e.path.size() + METADATA_SIZE;

  const auto dictionary_bytes = static_cast<uint32_t>(dictionary.size());
  const uint64_t data_base = HEADER_SIZE + dictionary_bytes + directory_bytes;

  std::vector<uint8_t> output;
  output.reserve(static_cast<size_t>(data_base + blobs.size()));

  put(output, CROM_MAGIC);
  put(output, CROM_VERSION);
  put(output, static_cast<uint32_t>(entries.size()));
  put(output, directory_bytes);
  put(output, dictionary_bytes);
  output.insert(output.end(), dictionary.begin(), dictionary.end());

  for (const auto &e : entries) {
    put(output, static_cast<uint16_t>(e.path.size()));
    output.insert(output.end(), e.path.begin(), e.path.end());
    const uint64_t absolute_offset = (e.flags & FLAG_DIRECTORY) ? 0 : data_base + e.blob_offset;
    put(output, absolute_offset);
    put(output, e.blob_size);
    put(output, e.uncompressed);
    output.push_back(e.flags);
  }

  output.insert(output.end(), blobs.begin(), blobs.end());

  std::ofstream out("cartridge.rom", std::ios::binary);
  if (!out) {
    std::cerr << "error: cannot create cartridge.rom\n";
    return 1;
  }
  out.write(reinterpret_cast<const char *>(output.data()), static_cast<std::streamsize>(output.size()));

  std::cout << "created cartridge.rom (" << entries.size() << " entries, "
            << output.size() << " bytes, dictionary=" << dictionary_bytes << "B)\n";

  return 0;
}
