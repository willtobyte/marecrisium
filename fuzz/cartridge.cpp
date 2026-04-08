#include <algorithm>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <physfs.h>
#include <zstd.h>

#include <ankerl/unordered_dense.h>

#include "../src/cartridge.cpp"

namespace {

struct state final {
  const uint8_t *data;
  size_t size;
  size_t position;
};

PHYSFS_sint64 mock_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 length) {
  auto *reader = static_cast<state *>(io->opaque);
  const auto remaining = reader->size - reader->position;
  if (remaining == 0)
    return 0;

  const auto count = std::min(static_cast<size_t>(length), remaining);
  std::memcpy(buffer, reader->data + reader->position, count);
  reader->position += count;
  return static_cast<PHYSFS_sint64>(count);
}

PHYSFS_sint64 mock_write(PHYSFS_Io *, const void *, PHYSFS_uint64) {
  return -1;
}

int mock_seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
  auto *reader = static_cast<state *>(io->opaque);
  if (offset > reader->size) [[unlikely]]
    return 0;

  reader->position = static_cast<size_t>(offset);
  return 1;
}

PHYSFS_sint64 mock_tell(PHYSFS_Io *io) {
  auto *reader = static_cast<state *>(io->opaque);
  return static_cast<PHYSFS_sint64>(reader->position);
}

PHYSFS_sint64 mock_length(PHYSFS_Io *io) {
  auto *reader = static_cast<state *>(io->opaque);
  return static_cast<PHYSFS_sint64>(reader->size);
}

PHYSFS_Io *mock_duplicate(PHYSFS_Io *io) {
  auto *reader = static_cast<state *>(io->opaque);
  auto *copy = new state{reader->data, reader->size, 0};
  auto *clone = new PHYSFS_Io(*io);
  clone->opaque = copy;
  return clone;
}

int mock_flush(PHYSFS_Io *) {
  return 1;
}

void mock_destroy(PHYSFS_Io *io) {
  delete static_cast<state *>(io->opaque);
  delete io;
}

PHYSFS_Io *make_io(const uint8_t *data, size_t size) {
  auto *reader = new state{data, size, 0};

  return new PHYSFS_Io{
    .version = 0,
    .opaque = reader,
    .read = mock_read,
    .write = mock_write,
    .seek = mock_seek,
    .tell = mock_tell,
    .length = mock_length,
    .duplicate = mock_duplicate,
    .flush = mock_flush,
    .destroy = mock_destroy,
  };
}

constexpr auto MAX_ENTRIES = 4096uz;

PHYSFS_EnumerateCallbackResult enumerate_callback(void *, const char *, const char *) {
  return PHYSFS_ENUM_OK;
}
}

static struct initializer final {
  initializer() noexcept { PHYSFS_init("fuzzer"); }
  ~initializer() noexcept { PHYSFS_deinit(); }
} init;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 12)
    return 0;

  uint32_t count;
  std::memcpy(&count, data + 8, sizeof(count));
  if (count > MAX_ENTRIES)
    return 0;

  auto *io = make_io(data, size);

  int claimed = 0;
  auto *opaque = crom_open_archive(io, "", 0, &claimed);

  if (!opaque) [[unlikely]] {
    if (!claimed)
      io->destroy(io);
    return 0;
  }

  auto *arc = static_cast<archive *>(opaque);

  for (const auto &item : arc->entries) {
    PHYSFS_Stat stat;
    crom_stat(opaque, item.path.c_str(), &stat);

    if (!(item.flags & FLAG_DIRECTORY)) {
      auto *file = crom_open_read(opaque, item.path.c_str());
      if (file) [[likely]] {
        uint8_t buffer[256];
        file->read(file, buffer, sizeof(buffer));
        file->destroy(file);
      }
    }
  }

  crom_enumerate(opaque, "", enumerate_callback, "", nullptr);
  crom_close_archive(opaque);

  return 0;
}

#if defined(FUZZ_CORPUS_RUNNER)
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <corpus_dir>\n";
    return 1;
  }

  int count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(argv[1])) {
    if (!entry.is_regular_file())
      continue;

    std::ifstream file(entry.path(), std::ios::binary);
    std::vector<uint8_t> contents(
      (std::istreambuf_iterator<char>(file)),
      std::istreambuf_iterator<char>());

    LLVMFuzzerTestOneInput(contents.data(), contents.size());
    ++count;
  }

  std::cout << "processed " << count << " corpus files\n";
  return 0;
}
#endif
