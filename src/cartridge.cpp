#include "cartridge.hpp"

namespace {
using namespace std::string_view_literals;

constexpr uint32_t CROM_MAGIC = 0x43524F4D;
constexpr uint32_t CROM_VERSION = 1;
constexpr uint8_t FLAG_DIRECTORY = 1;
constexpr uint64_t MAX_UNCOMPRESSED = 64 * 1024 * 1024;
constexpr uint32_t MAX_ENTRIES = 1u << 20;

struct entry final {
  std::string_view path;
  uint64_t offset;
  uint64_t compressed;
  uint64_t uncompressed;
  uint8_t flags;
};

struct archive final {
  PHYSFS_Io *io{nullptr};
  ZSTD_DCtx *dctx{nullptr};
  PHYSFS_uint64 bytes{0};
  std::vector<char> paths;
  std::vector<uint8_t> chunk;
  std::vector<entry> entries;
  ankerl::unordered_dense::map<std::string_view, size_t> index;
  ankerl::unordered_dense::map<std::string_view, std::vector<const char *>> children;

  ~archive() {
    if (dctx)
      ZSTD_freeDCtx(dctx);
  }
};

struct handle final {
  std::shared_ptr<uint8_t[]> data;
  size_t size;
  uint64_t position;
};

template <std::integral T>
[[nodiscard]] T read(const uint8_t *p) noexcept {
  T value;
  std::memcpy(&value, p, sizeof(T));
  return value;
}

[[nodiscard]] bool drain(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 length) noexcept {
  auto *destination = static_cast<uint8_t *>(buffer);
  while (length > 0) {
    const auto read = io->read(io, destination, length);
    if (read <= 0) [[unlikely]]
      return false;

    destination += read;
    length -= static_cast<PHYSFS_uint64>(read);
  }

  return true;
}

PHYSFS_sint64 file_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 length) {
  auto *reader = static_cast<handle *>(io->opaque);
  const auto remaining = reader->size - reader->position;
  if (remaining == 0)
    return 0;

  const auto count = std::min(length, static_cast<PHYSFS_uint64>(remaining));
  std::memcpy(buffer, reader->data.get() + reader->position, static_cast<size_t>(count));
  reader->position += count;
  return static_cast<PHYSFS_sint64>(count);
}

PHYSFS_sint64 file_write(PHYSFS_Io *, const void *, PHYSFS_uint64) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return -1;
}

int file_seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
  auto *reader = static_cast<handle *>(io->opaque);
  if (offset > reader->size) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_PAST_EOF);
    return 0;
  }

  reader->position = offset;
  return 1;
}

PHYSFS_sint64 file_tell(PHYSFS_Io *io) {
  auto *reader = static_cast<handle *>(io->opaque);
  return static_cast<PHYSFS_sint64>(reader->position);
}

PHYSFS_sint64 file_length(PHYSFS_Io *io) {
  auto *reader = static_cast<handle *>(io->opaque);
  return static_cast<PHYSFS_sint64>(reader->size);
}

PHYSFS_Io *file_duplicate(PHYSFS_Io *io) {
  auto *reader = static_cast<handle *>(io->opaque);
  auto copy = std::make_unique<handle>(reader->data, reader->size, uint64_t{0});
  auto clone = std::make_unique<PHYSFS_Io>(*io);

  clone->opaque = copy.release();
  return clone.release();
}

int file_flush(PHYSFS_Io *) {
  return 1;
}

void file_destroy(PHYSFS_Io *io) {
  delete static_cast<handle *>(io->opaque);
  delete io;
}

void *crom_open_archive(PHYSFS_Io *io, const char *, int, int *claimed) {
  uint8_t header[12];
  if (!io->seek(io, 0) || !drain(io, header, sizeof(header))) [[unlikely]]
    return nullptr;

  const auto magic = read<uint32_t>(header);
  if (magic != CROM_MAGIC)
    return nullptr;

  *claimed = 1;

  const auto version = read<uint32_t>(header + 4);
  if (version != CROM_VERSION) [[unlikely]] {
    io->destroy(io);
    PHYSFS_setErrorCode(PHYSFS_ERR_UNSUPPORTED);
    return nullptr;
  }

  const auto count = read<uint32_t>(header + 8);
  if (count > MAX_ENTRIES) [[unlikely]] {
    io->destroy(io);
    PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
    return nullptr;
  }

  auto arc = std::make_unique<archive>();
  arc->io = io;
  arc->dctx = ZSTD_createDCtx();
  arc->index.reserve(count);
  arc->entries.reserve(count);

  struct pending final {
    uint32_t path_offset;
    uint16_t path_length;
    uint64_t offset;
    uint64_t compressed;
    uint64_t uncompressed;
    uint8_t flags;
  };

  std::vector<pending> staging;
  staging.reserve(count);
  arc->paths.reserve(static_cast<size_t>(count) * 25);

  std::vector<uint8_t> buffer;
  for (uint32_t i = 0; i < count; ++i) {
    uint8_t preamble[2];
    if (!drain(io, preamble, 2)) [[unlikely]] {
      io->destroy(io);
      return nullptr;
    }

    const auto length = read<uint16_t>(preamble);
    if (length == 0) [[unlikely]] {
      io->destroy(io);
      PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
      return nullptr;
    }

    const auto size = static_cast<size_t>(length) + 25;
    buffer.resize(size);
    if (!drain(io, buffer.data(), size)) [[unlikely]] {
      io->destroy(io);
      return nullptr;
    }

    const auto *metadata = buffer.data() + length;
    const auto path_offset = static_cast<uint32_t>(arc->paths.size());
    arc->paths.insert(arc->paths.end(), buffer.data(), buffer.data() + length);
    arc->paths.push_back('\0');

    staging.push_back(pending{
      path_offset,
      length,
      read<uint64_t>(metadata),
      read<uint64_t>(metadata + 8),
      read<uint64_t>(metadata + 16),
      metadata[24],
    });
  }

  const auto *base = arc->paths.data();
  for (uint32_t i = 0; i < count; ++i) {
    const auto &staged = staging[i];
    const std::string_view path{base + staged.path_offset, staged.path_length};

    arc->entries.push_back(entry{path, staged.offset, staged.compressed, staged.uncompressed, staged.flags});
    arc->index.emplace(path, i);

    const auto slash = path.rfind('/');
    const auto parent = slash == std::string_view::npos ? ""sv : path.substr(0, slash);
    const auto leaf_offset = slash == std::string_view::npos ? 0u : static_cast<uint32_t>(slash + 1);
    arc->children[parent].emplace_back(base + staged.path_offset + leaf_offset);
  }

  const auto total = io->length(io);
  arc->bytes = total < 0 ? 0 : static_cast<PHYSFS_uint64>(total);

  return arc.release();
}

PHYSFS_EnumerateCallbackResult crom_enumerate(void *opaque, const char *dirname, PHYSFS_EnumerateCallback callback, const char *origdir, void *cbdata) {
  auto *arc = static_cast<archive *>(opaque);
  const std::string_view dir = dirname;

  const auto it = arc->children.find(dir);
  if (it == arc->children.end())
    return PHYSFS_ENUM_OK;

  for (const auto *name : it->second) {
    const auto result = callback(cbdata, origdir, name);
    if (result == PHYSFS_ENUM_ERROR)
      return PHYSFS_ENUM_ERROR;
    if (result == PHYSFS_ENUM_STOP)
      return PHYSFS_ENUM_STOP;
  }

  return PHYSFS_ENUM_OK;
}

PHYSFS_Io *crom_open_read(void *opaque, const char *name) {
  auto *arc = static_cast<archive *>(opaque);

  const auto it = arc->index.find(name);
  if (it == arc->index.end()) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
    return nullptr;
  }

  const auto &found = arc->entries[it->second];

  if (found.flags & FLAG_DIRECTORY) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_A_FILE);
    return nullptr;
  }

  const auto bytes = arc->bytes;
  if (found.offset > bytes ||
      found.compressed > bytes - found.offset) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
    return nullptr;
  }

  if (found.uncompressed > MAX_UNCOMPRESSED) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
    return nullptr;
  }

  if (!arc->io->seek(arc->io, found.offset)) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_IO);
    return nullptr;
  }

  const auto uncompressed = static_cast<size_t>(found.uncompressed);

  auto buffer = std::make_shared_for_overwrite<uint8_t[]>(uncompressed);

  if (uncompressed > 0) [[likely]] {
    ZSTD_DCtx_reset(arc->dctx, ZSTD_reset_session_only);

    const auto capacity = ZSTD_DStreamInSize();
    if (arc->chunk.size() < capacity)
      arc->chunk.resize(capacity);
    auto *chunk = arc->chunk.data();
    ZSTD_outBuffer out{buffer.get(), uncompressed, 0};
    auto remaining = static_cast<PHYSFS_uint64>(found.compressed);

    while (remaining > 0) {
      const auto want = std::min(remaining, static_cast<PHYSFS_uint64>(capacity));
      const auto read = arc->io->read(arc->io, chunk, want);
      if (read <= 0) [[unlikely]] {
        PHYSFS_setErrorCode(PHYSFS_ERR_IO);
        return nullptr;
      }

      ZSTD_inBuffer in{chunk, static_cast<size_t>(read), 0};
      while (in.pos < in.size) {
        const auto result = ZSTD_decompressStream(arc->dctx, &out, &in);
        if (ZSTD_isError(result)) [[unlikely]] {
          PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
          return nullptr;
        }
      }

      remaining -= static_cast<PHYSFS_uint64>(read);
    }

    if (out.pos != uncompressed) [[unlikely]] {
      PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
      return nullptr;
    }
  }

  auto reader = std::make_unique<handle>(std::move(buffer), uncompressed, uint64_t{0});
  auto io_out = std::make_unique<PHYSFS_Io>(PHYSFS_Io{
    .version = 0,
    .opaque = reader.get(),
    .read = file_read,
    .write = file_write,
    .seek = file_seek,
    .tell = file_tell,
    .length = file_length,
    .duplicate = file_duplicate,
    .flush = file_flush,
    .destroy = file_destroy,
  });

  reader.release();
  return io_out.release();
}

PHYSFS_Io *crom_open_write(void *, const char *) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return nullptr;
}

PHYSFS_Io *crom_open_append(void *, const char *) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return nullptr;
}

int crom_remove(void *, const char *) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return 0;
}

int crom_mkdir(void *, const char *) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return 0;
}

int crom_stat(void *opaque, const char *name, PHYSFS_Stat *stat) {
  auto *arc = static_cast<archive *>(opaque);
  const std::string_view path = name;

  const auto it = arc->index.find(path);
  if (it == arc->index.end()) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
    return 0;
  }

  const auto &e = arc->entries[it->second];
  const auto directory = (e.flags & FLAG_DIRECTORY) != 0;
  stat->filesize = directory
    ? -1
    : static_cast<PHYSFS_sint64>(e.uncompressed);
  stat->modtime = -1;
  stat->createtime = -1;
  stat->accesstime = -1;
  stat->filetype = directory
    ? PHYSFS_FILETYPE_DIRECTORY
    : PHYSFS_FILETYPE_REGULAR;
  stat->readonly = 1;

  return 1;
}

void crom_close_archive(void *opaque) {
  auto *arc = static_cast<archive *>(opaque);
  arc->io->destroy(arc->io);
  delete arc;
}
}

const PHYSFS_Archiver archiver = {
  0,
  {
    "ROM",
    "Carimbo ROM archive",
    "Carimbo",
    "https://willtobyte.net",
    0,
  },
  crom_open_archive,
  crom_enumerate,
  crom_open_read,
  crom_open_write,
  crom_open_append,
  crom_remove,
  crom_mkdir,
  crom_stat,
  crom_close_archive,
};
