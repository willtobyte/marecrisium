#include "cartridge.hpp"

namespace {
constexpr uint32_t CROM_MAGIC = 0x43524F4D;
constexpr uint32_t CROM_VERSION = 1;
constexpr uint8_t FLAG_DIRECTORY = 1;

struct entry final {
  std::string path;
  uint64_t data_offset;
  uint64_t compressed_size;
  uint64_t uncompressed_size;
  uint8_t flags;
};

struct archive final {
  PHYSFS_Io *io;
  ZSTD_DCtx *dctx;
  std::vector<entry> entries;
  ankerl::unordered_dense::map<std::string_view, size_t, transparent_hash, std::equal_to<>> index;
  ankerl::unordered_dense::map<std::string_view, std::vector<std::string_view>, transparent_hash, std::equal_to<>> children;
};

struct handle final {
  std::shared_ptr<uint8_t[]> data;
  size_t size;
  uint64_t position;
};

uint16_t read_u16(const uint8_t *pointer) noexcept {
  return static_cast<uint16_t>(
    static_cast<uint16_t>(pointer[0]) |
    static_cast<uint16_t>(static_cast<uint16_t>(pointer[1]) << 8));
}

uint32_t read_u32(const uint8_t *pointer) noexcept {
  return static_cast<uint32_t>(pointer[0]) |
         (static_cast<uint32_t>(pointer[1]) << 8) |
         (static_cast<uint32_t>(pointer[2]) << 16) |
         (static_cast<uint32_t>(pointer[3]) << 24);
}

uint64_t read_u64(const uint8_t *pointer) noexcept {
  return static_cast<uint64_t>(pointer[0]) |
         (static_cast<uint64_t>(pointer[1]) << 8) |
         (static_cast<uint64_t>(pointer[2]) << 16) |
         (static_cast<uint64_t>(pointer[3]) << 24) |
         (static_cast<uint64_t>(pointer[4]) << 32) |
         (static_cast<uint64_t>(pointer[5]) << 40) |
         (static_cast<uint64_t>(pointer[6]) << 48) |
         (static_cast<uint64_t>(pointer[7]) << 56);
}

bool read_all(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 length) {
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

std::string_view parent_dir(std::string_view path) noexcept {
  const auto position = path.rfind('/');
  if (position == std::string_view::npos)
    return {};

  return path.substr(0, position);
}

std::string_view filename(std::string_view path) noexcept {
  const auto position = path.rfind('/');
  if (position == std::string_view::npos)
    return path;

  return path.substr(position + 1);
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

  auto *copy = new (std::nothrow) handle{reader->data, reader->size, 0};
  if (!copy) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }

  auto *clone = new (std::nothrow) PHYSFS_Io{};
  if (!clone) [[unlikely]] {
    delete copy;
    PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }

  *clone = *io;
  clone->opaque = copy;
  return clone;
}

int file_flush(PHYSFS_Io *) {
  return 1;
}

void file_destroy(PHYSFS_Io *io) {
  delete static_cast<handle *>(io->opaque);
  delete io;
}

void *crom_open_archive(PHYSFS_Io *io, const char *, int for_write, int *claimed) {
  if (for_write) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
    return nullptr;
  }

  uint8_t header[12];
  if (!io->seek(io, 0) || !read_all(io, header, sizeof(header)))
    return nullptr;

  const auto magic = read_u32(header);
  if (magic != CROM_MAGIC)
    return nullptr;

  *claimed = 1;

  const auto version = read_u32(header + 4);
  if (version != CROM_VERSION) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_UNSUPPORTED);
    return nullptr;
  }

  const auto entry_count = read_u32(header + 8);
  auto *arc = new (std::nothrow) archive{};
  if (!arc) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }

  arc->io = io;
  arc->dctx = ZSTD_createDCtx();
  if (!arc->dctx) [[unlikely]] {
    delete arc;
    PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }

  arc->entries.reserve(entry_count);

  std::vector<uint8_t> entry_buffer;
  for (uint32_t i = 0; i < entry_count; ++i) {
    uint8_t length_buffer[2];
    if (!read_all(io, length_buffer, 2)) [[unlikely]] {
      ZSTD_freeDCtx(arc->dctx);
      delete arc;
      return nullptr;
    }

    const auto path_length = read_u16(length_buffer);
    const auto entry_size = static_cast<size_t>(path_length + 25);

    entry_buffer.resize(entry_size);
    if (!read_all(io, entry_buffer.data(), entry_size)) [[unlikely]] {
      ZSTD_freeDCtx(arc->dctx);
      delete arc;
      return nullptr;
    }

    const auto *metadata = entry_buffer.data() + path_length;

    entry item{
      std::string(reinterpret_cast<const char *>(entry_buffer.data()), path_length),
      read_u64(metadata),
      read_u64(metadata + 8),
      read_u64(metadata + 16),
      metadata[24]
    };

    arc->entries.push_back(std::move(item));
  }

  arc->index.reserve(arc->entries.size());
  for (size_t i = 0; i < arc->entries.size(); ++i) {
    const auto &e = arc->entries[i];
    arc->index.emplace(std::string_view{e.path}, i);
    arc->children[parent_dir(e.path)].emplace_back(filename(e.path));
  }

  return arc;
}

PHYSFS_EnumerateCallbackResult crom_enumerate(void *opaque, const char *dirname, PHYSFS_EnumerateCallback callback, const char *origdir, void *cbdata) {
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

PHYSFS_Io *crom_open_read(void *opaque, const char *name) {
  auto *arc = static_cast<archive *>(opaque);
  const std::string_view path = name;

  const auto it = arc->index.find(path);
  if (it == arc->index.end()) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
    return nullptr;
  }

  const auto &found = arc->entries[it->second];

  if (found.flags & FLAG_DIRECTORY) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_A_FILE);
    return nullptr;
  }

  const auto compressed_size = static_cast<size_t>(found.compressed_size);
  const auto uncompressed_size = static_cast<size_t>(found.uncompressed_size);

  auto compressed = std::make_unique_for_overwrite<uint8_t[]>(compressed_size);
  if (!arc->io->seek(arc->io, found.data_offset) ||
      !read_all(arc->io, compressed.get(), found.compressed_size)) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_IO);
    return nullptr;
  }

  auto buffer = std::make_shared_for_overwrite<uint8_t[]>(uncompressed_size);

  auto *reader = new (std::nothrow) handle{};
  if (!reader) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }

  reader->size = uncompressed_size;
  reader->position = 0;

  if (uncompressed_size > 0) {
    const auto result = ZSTD_decompressDCtx(
      arc->dctx,
      buffer.get(), uncompressed_size,
      compressed.get(), compressed_size);

    if (ZSTD_isError(result)) [[unlikely]] {
      delete reader;
      PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
      return nullptr;
    }
  }

  reader->data = std::move(buffer);

  auto *io = new (std::nothrow) PHYSFS_Io{};
  if (!io) [[unlikely]] {
    delete reader;
    PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }

  io->version = 0;
  io->opaque = reader;
  io->read = file_read;
  io->write = file_write;
  io->seek = file_seek;
  io->tell = file_tell;
  io->length = file_length;
  io->duplicate = file_duplicate;
  io->flush = file_flush;
  io->destroy = file_destroy;

  return io;
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
  stat->filesize = (e.flags & FLAG_DIRECTORY)
    ? -1
    : static_cast<PHYSFS_sint64>(e.uncompressed_size);
  stat->modtime = -1;
  stat->createtime = -1;
  stat->accesstime = -1;
  stat->filetype = (e.flags & FLAG_DIRECTORY)
    ? PHYSFS_FILETYPE_DIRECTORY
    : PHYSFS_FILETYPE_REGULAR;
  stat->readonly = 1;

  return 1;
}

void crom_close_archive(void *opaque) {
  auto *arc = static_cast<archive *>(opaque);
  if (arc->io)
    arc->io->destroy(arc->io);

  if (arc->dctx)
    ZSTD_freeDCtx(arc->dctx);

  delete arc;
}
}

const PHYSFS_Archiver archiver = {
  0,
  {
    "ROM",
    "Carimbo ROM archive (ZStandard)",
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
