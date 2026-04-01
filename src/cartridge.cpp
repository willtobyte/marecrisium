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
  std::vector<entry> entries;
  ankerl::unordered_dense::map<std::string, size_t, transparent_hash, std::equal_to<>> index;
  ankerl::unordered_dense::map<std::string, std::vector<std::string>, transparent_hash, std::equal_to<>> children;
};

struct handle final {
  std::vector<uint8_t> data;
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
  auto *h = static_cast<handle *>(io->opaque);
  const auto remaining = h->data.size() - h->position;
  if (remaining == 0)
    return 0;

  const auto count = std::min(length, static_cast<PHYSFS_uint64>(remaining));
  std::memcpy(buffer, h->data.data() + h->position, static_cast<size_t>(count));
  h->position += count;
  return static_cast<PHYSFS_sint64>(count);
}

PHYSFS_sint64 file_write(PHYSFS_Io *, const void *, PHYSFS_uint64) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return -1;
}

int file_seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
  auto *h = static_cast<handle *>(io->opaque);
  if (offset > h->data.size()) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_PAST_EOF);
    return 0;
  }

  h->position = offset;
  return 1;
}

PHYSFS_sint64 file_tell(PHYSFS_Io *io) {
  auto *h = static_cast<handle *>(io->opaque);
  return static_cast<PHYSFS_sint64>(h->position);
}

PHYSFS_sint64 file_length(PHYSFS_Io *io) {
  auto *h = static_cast<handle *>(io->opaque);
  return static_cast<PHYSFS_sint64>(h->data.size());
}

PHYSFS_Io *file_duplicate(PHYSFS_Io *io) {
  auto *h = static_cast<handle *>(io->opaque);

  auto *copy = new (std::nothrow) handle{h->data, 0};
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
  arc->entries.reserve(entry_count);

  for (uint32_t i = 0; i < entry_count; ++i) {
    uint8_t buffer[2];
    if (!read_all(io, buffer, 2)) [[unlikely]] {
      delete arc;
      return nullptr;
    }
    const auto length = read_u16(buffer);

    std::string path(length, '\0');
    if (!read_all(io, path.data(), length)) [[unlikely]] {
      delete arc;
      return nullptr;
    }

    uint8_t metadata[25];
    if (!read_all(io, metadata, sizeof(metadata))) [[unlikely]] {
      delete arc;
      return nullptr;
    }

    arc->entries.push_back({
      std::move(path),
      read_u64(metadata),
      read_u64(metadata + 8),
      read_u64(metadata + 16),
      metadata[24]
    });
  }

  arc->index.reserve(arc->entries.size());
  for (size_t i = 0; i < arc->entries.size(); ++i) {
    const auto &e = arc->entries[i];
    arc->index.emplace(e.path, i);
    arc->children[std::string(parent_dir(e.path))].emplace_back(filename(e.path));
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
    const auto result = callback(cbdata, origdir, name.c_str());
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

  std::vector<uint8_t> compressed(static_cast<size_t>(found.compressed_size));
  if (!arc->io->seek(arc->io, found.data_offset) ||
      !read_all(arc->io, compressed.data(), found.compressed_size)) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_IO);
    return nullptr;
  }

  auto *h = new (std::nothrow) handle{};
  if (!h) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }

  h->data.resize(static_cast<size_t>(found.uncompressed_size));
  h->position = 0;

  if (found.uncompressed_size > 0) {
    const auto result = ZSTD_decompress(
      h->data.data(), h->data.size(),
      compressed.data(), compressed.size());

    if (ZSTD_isError(result)) [[unlikely]] {
      delete h;
      PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
      return nullptr;
    }
  }

  auto *io = new (std::nothrow) PHYSFS_Io{};
  if (!io) [[unlikely]] {
    delete h;
    PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
    return nullptr;
  }

  io->version = 0;
  io->opaque = h;
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
