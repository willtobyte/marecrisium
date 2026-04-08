#include "cartridge.hpp"

namespace {
using namespace std::string_view_literals;

constexpr uint32_t CROM_MAGIC = 0x43524F4D;
constexpr uint32_t CROM_VERSION = 1;
constexpr uint8_t FLAG_DIRECTORY = 1;

struct entry final {
  std::string path;
  uint64_t offset;
  uint64_t compressed;
  uint64_t uncompressed;
  uint8_t flags;
};

struct archive final {
  PHYSFS_Io *io{nullptr};
  ZSTD_DCtx *dctx{nullptr};
  std::vector<entry> entries;
  ankerl::unordered_dense::map<std::string_view, size_t> index;
  ankerl::unordered_dense::map<std::string_view, std::vector<std::string_view>> children;

  ~archive() noexcept {
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
static T read(const uint8_t *p) noexcept {
  T value;
  std::memcpy(&value, p, sizeof(T));
  return value;
}

static bool drain(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 length) {
  auto *destination = static_cast<uint8_t *>(buffer);
  while (length > 0) {
    const auto got = io->read(io, destination, length);
    if (got <= 0) [[unlikely]]
      return false;

    destination += got;
    length -= static_cast<PHYSFS_uint64>(got);
  }

  return true;
}

static std::string_view parent_dir(std::string_view path) noexcept {
  const auto position = path.rfind('/');
  if (position == std::string_view::npos)
    return ""sv;

  return path.substr(0, position);
}

static std::string_view filename(std::string_view path) noexcept {
  const auto position = path.rfind('/');
  if (position == std::string_view::npos)
    return path;

  return path.substr(position + 1);
}

static PHYSFS_sint64 file_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 length) {
  auto *reader = static_cast<handle *>(io->opaque);
  const auto remaining = reader->size - reader->position;
  if (remaining == 0)
    return 0;

  const auto count = std::min(length, static_cast<PHYSFS_uint64>(remaining));
  std::memcpy(buffer, reader->data.get() + reader->position, static_cast<size_t>(count));
  reader->position += count;
  return static_cast<PHYSFS_sint64>(count);
}

static PHYSFS_sint64 file_write(PHYSFS_Io *, const void *, PHYSFS_uint64) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return -1;
}

static int file_seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
  auto *reader = static_cast<handle *>(io->opaque);
  if (offset > reader->size) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_PAST_EOF);
    return 0;
  }

  reader->position = offset;
  return 1;
}

static PHYSFS_sint64 file_tell(PHYSFS_Io *io) {
  auto *reader = static_cast<handle *>(io->opaque);
  return static_cast<PHYSFS_sint64>(reader->position);
}

static PHYSFS_sint64 file_length(PHYSFS_Io *io) {
  auto *reader = static_cast<handle *>(io->opaque);
  return static_cast<PHYSFS_sint64>(reader->size);
}

static PHYSFS_Io *file_duplicate(PHYSFS_Io *io) {
  auto *reader = static_cast<handle *>(io->opaque);
  auto *copy = new handle{reader->data, reader->size, 0};
  auto *clone = new PHYSFS_Io(*io);

  clone->opaque = copy;
  return clone;
}

static int file_flush(PHYSFS_Io *) {
  return 1;
}

static void file_destroy(PHYSFS_Io *io) {
  delete static_cast<handle *>(io->opaque);
  delete io;
}

static void *crom_open_archive(PHYSFS_Io *io, const char *, int for_write, int *claimed) {
  if (for_write) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
    return nullptr;
  }

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

  auto arc = std::make_unique<archive>();
  arc->io = io;
  arc->dctx = ZSTD_createDCtx();
  arc->entries.reserve(count);
  arc->index.reserve(count);

  std::vector<uint8_t> buffer;
  for (uint32_t i = 0; i < count; ++i) {
    uint8_t preamble[2];
    if (!drain(io, preamble, 2)) [[unlikely]] {
      io->destroy(io);
      return nullptr;
    }

    const auto length = read<uint16_t>(preamble);
    const auto size = static_cast<size_t>(length + 25);

    buffer.resize(size);
    if (!drain(io, buffer.data(), size)) [[unlikely]] {
      io->destroy(io);
      return nullptr;
    }

    const auto *metadata = buffer.data() + length;

    auto &item = arc->entries.emplace_back(entry{
      std::string(reinterpret_cast<const char *>(buffer.data()), length),
      read<uint64_t>(metadata),
      read<uint64_t>(metadata + 8),
      read<uint64_t>(metadata + 16),
      metadata[24]
    });

    arc->index.emplace(item.path, i);
    arc->children[parent_dir(item.path)].emplace_back(filename(item.path));
  }

  return arc.release();
}

static PHYSFS_EnumerateCallbackResult crom_enumerate(void *opaque, const char *dirname, PHYSFS_EnumerateCallback callback, const char *origdir, void *cbdata) {
  auto *arc = static_cast<archive *>(opaque);
  const std::string_view dir = dirname;

  const auto it = arc->children.find(dir);
  if (it == arc->children.end())
    return PHYSFS_ENUM_OK;

  for (const auto &name : it->second) {
    const auto result = callback(cbdata, origdir, name.data());
    if (result == PHYSFS_ENUM_ERROR)
      return PHYSFS_ENUM_ERROR;
    if (result == PHYSFS_ENUM_STOP)
      return PHYSFS_ENUM_STOP;
  }

  return PHYSFS_ENUM_OK;
}

static PHYSFS_Io *crom_open_read(void *opaque, const char *name) {
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

  const auto length = arc->io->length(arc->io);
  if (length < 0 ||
      found.offset > static_cast<PHYSFS_uint64>(length) ||
      found.compressed > static_cast<PHYSFS_uint64>(length) - found.offset) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
    return nullptr;
  }

  const auto size = static_cast<size_t>(found.compressed);

  auto compressed = std::make_unique_for_overwrite<uint8_t[]>(size);

  if (!arc->io->seek(arc->io, found.offset) ||
      !drain(arc->io, compressed.get(), found.compressed)) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_IO);
    return nullptr;
  }

  const auto expected = ZSTD_getFrameContentSize(compressed.get(), size);
  if (expected == ZSTD_CONTENTSIZE_ERROR ||
      expected == ZSTD_CONTENTSIZE_UNKNOWN ||
      expected != found.uncompressed) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
    return nullptr;
  }

  const auto uncompressed = static_cast<size_t>(found.uncompressed);

  auto buffer = std::make_shared_for_overwrite<uint8_t[]>(uncompressed);

  if (uncompressed > 0) [[likely]] {
    const auto result = ZSTD_decompressDCtx(
      arc->dctx,
      buffer.get(),
      uncompressed,
      compressed.get(),
      size);

    if (ZSTD_isError(result)) [[unlikely]] {
      PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
      return nullptr;
    }
  }

  auto *reader = new handle{std::move(buffer), uncompressed, 0};

  return new PHYSFS_Io{
    .version = 0,
    .opaque = reader,
    .read = file_read,
    .write = file_write,
    .seek = file_seek,
    .tell = file_tell,
    .length = file_length,
    .duplicate = file_duplicate,
    .flush = file_flush,
    .destroy = file_destroy,
  };
}

static PHYSFS_Io *crom_open_write(void *, const char *) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return nullptr;
}

static PHYSFS_Io *crom_open_append(void *, const char *) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return nullptr;
}

static int crom_remove(void *, const char *) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return 0;
}

static int crom_mkdir(void *, const char *) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return 0;
}

static int crom_stat(void *opaque, const char *name, PHYSFS_Stat *stat) {
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

static void crom_close_archive(void *opaque) {
  auto *arc = static_cast<archive *>(opaque);
  if (arc->io) [[likely]]
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
