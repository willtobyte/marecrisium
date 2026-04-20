#include "cartridge.hpp"

namespace {
using namespace std::string_view_literals;

constexpr uint32_t CROM_MAGIC = 0x43524F4D;
constexpr uint32_t CROM_VERSION = 2;
constexpr uint8_t FLAG_DIRECTORY = 1;
constexpr uint64_t MAX_UNCOMPRESSED = 64 * 1024 * 1024;
constexpr uint32_t MAX_ENTRIES = 1u << 20;
constexpr uint64_t MAX_DIRECTORY_BYTES = 256 * 1024 * 1024;
constexpr uint32_t MAX_DICTIONARY_BYTES = 16 * 1024 * 1024;
constexpr size_t HEADER_SIZE = 24;
constexpr size_t METADATA_SIZE = 25;

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
  ZSTD_DDict *ddict{nullptr};
  PHYSFS_uint64 bytes{0};
  std::vector<char> paths;
  std::vector<uint8_t> compressed;
  std::vector<entry> entries;
  ankerl::unordered_dense::map<std::string_view, size_t> index;
  ankerl::unordered_dense::map<std::string_view, std::vector<const char *>> children;

  ~archive() {
    if (ddict)
      ZSTD_freeDDict(ddict);
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
    const auto got = io->read(io, destination, length);
    if (got <= 0) [[unlikely]]
      return false;

    destination += got;
    length -= static_cast<PHYSFS_uint64>(got);
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
  return static_cast<PHYSFS_sint64>(static_cast<handle *>(io->opaque)->position);
}

PHYSFS_sint64 file_length(PHYSFS_Io *io) {
  return static_cast<PHYSFS_sint64>(static_cast<handle *>(io->opaque)->size);
}

PHYSFS_Io *file_duplicate(PHYSFS_Io *io) {
  auto *reader = static_cast<handle *>(io->opaque);
  auto *copy = new handle{reader->data, reader->size, uint64_t{0}};
  auto *clone = new PHYSFS_Io(*io);
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

void *crom_open_archive(PHYSFS_Io *io, const char *, int, int *claimed) {
  uint8_t header[HEADER_SIZE];
  if (!io->seek(io, 0) || !drain(io, header, sizeof(header))) [[unlikely]]
    return nullptr;

  if (read<uint32_t>(header) != CROM_MAGIC)
    return nullptr;

  *claimed = 1;

  const auto fail = [&](PHYSFS_ErrorCode code) {
    io->destroy(io);
    PHYSFS_setErrorCode(code);
    return nullptr;
  };

  if (read<uint32_t>(header + 4) != CROM_VERSION) [[unlikely]]
    return fail(PHYSFS_ERR_UNSUPPORTED);

  const auto count = read<uint32_t>(header + 8);
  const auto directory_bytes = read<uint64_t>(header + 12);
  const auto dictionary_bytes = read<uint32_t>(header + 20);
  if (count > MAX_ENTRIES || directory_bytes > MAX_DIRECTORY_BYTES || dictionary_bytes > MAX_DICTIONARY_BYTES) [[unlikely]]
    return fail(PHYSFS_ERR_CORRUPT);

  std::vector<uint8_t> dictionary(dictionary_bytes);
  if (dictionary_bytes > 0 && !drain(io, dictionary.data(), dictionary_bytes)) [[unlikely]] {
    io->destroy(io);
    return nullptr;
  }

  std::vector<uint8_t> toc(static_cast<size_t>(directory_bytes));
  if (directory_bytes > 0 && !drain(io, toc.data(), directory_bytes)) [[unlikely]] {
    io->destroy(io);
    return nullptr;
  }

  auto arc = std::make_unique<archive>();
  arc->io = io;
  arc->dctx = ZSTD_createDCtx();
  if (dictionary_bytes > 0)
    arc->ddict = ZSTD_createDDict(dictionary.data(), dictionary_bytes);
  arc->entries.reserve(count);
  arc->index.reserve(count);
  arc->paths.reserve(static_cast<size_t>(directory_bytes));

  struct slot final {
    uint32_t offset;
    uint16_t length;
  };
  std::vector<slot> slots;
  slots.reserve(count);

  const auto *cursor = toc.data();
  const auto *end = cursor + toc.size();

  for (uint32_t i = 0; i < count; ++i) {
    if (static_cast<size_t>(end - cursor) < 2u) [[unlikely]]
      return fail(PHYSFS_ERR_CORRUPT);

    const auto length = read<uint16_t>(cursor);
    cursor += 2;

    if (length == 0 || static_cast<size_t>(end - cursor) < static_cast<size_t>(length) + METADATA_SIZE) [[unlikely]]
      return fail(PHYSFS_ERR_CORRUPT);

    slots.push_back({static_cast<uint32_t>(arc->paths.size()), length});
    arc->paths.insert(arc->paths.end(), cursor, cursor + length);
    arc->paths.push_back('\0');

    const auto *metadata = cursor + length;
    arc->entries.push_back(entry{
      {},
      read<uint64_t>(metadata),
      read<uint64_t>(metadata + 8),
      read<uint64_t>(metadata + 16),
      metadata[24],
    });

    cursor += static_cast<size_t>(length) + METADATA_SIZE;
  }

  // Bind path string_views now that arc->paths won't reallocate again.
  const auto *base = arc->paths.data();
  for (uint32_t i = 0; i < count; ++i) {
    auto &e = arc->entries[i];
    e.path = std::string_view{base + slots[i].offset, slots[i].length};

    arc->index.emplace(e.path, i);

    const auto slash = e.path.rfind('/');
    const auto parent = slash == std::string_view::npos ? ""sv : e.path.substr(0, slash);
    const auto leaf = slash == std::string_view::npos ? 0u : slash + 1;
    arc->children[parent].emplace_back(e.path.data() + leaf);
  }

  const auto total = io->length(io);
  arc->bytes = total < 0 ? 0 : static_cast<PHYSFS_uint64>(total);

  return arc.release();
}

PHYSFS_EnumerateCallbackResult crom_enumerate(void *opaque, const char *dirname, PHYSFS_EnumerateCallback callback, const char *origdir, void *cbdata) {
  auto *arc = static_cast<archive *>(opaque);

  const auto it = arc->children.find(dirname);
  if (it == arc->children.end())
    return PHYSFS_ENUM_OK;

  for (const auto *name : it->second) {
    const auto result = callback(cbdata, origdir, name);
    if (result != PHYSFS_ENUM_OK)
      return result;
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

  if (found.uncompressed > MAX_UNCOMPRESSED ||
      found.offset > arc->bytes ||
      found.compressed > arc->bytes - found.offset) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
    return nullptr;
  }

  const auto uncompressed = static_cast<size_t>(found.uncompressed);
  const auto compressed = static_cast<size_t>(found.compressed);

  auto buffer = std::make_shared_for_overwrite<uint8_t[]>(uncompressed);

  if (uncompressed > 0) [[likely]] {
    if (!arc->io->seek(arc->io, found.offset)) [[unlikely]] {
      PHYSFS_setErrorCode(PHYSFS_ERR_IO);
      return nullptr;
    }

    if (arc->compressed.size() < compressed)
      arc->compressed.resize(compressed);

    if (!drain(arc->io, arc->compressed.data(), compressed)) [[unlikely]] {
      PHYSFS_setErrorCode(PHYSFS_ERR_IO);
      return nullptr;
    }

    const auto result = arc->ddict
      ? ZSTD_decompress_usingDDict(
          arc->dctx,
          buffer.get(), uncompressed,
          arc->compressed.data(), compressed,
          arc->ddict)
      : ZSTD_decompressDCtx(
          arc->dctx,
          buffer.get(), uncompressed,
          arc->compressed.data(), compressed
        );

    if (ZSTD_isError(result) || result != uncompressed) [[unlikely]] {
      PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
      return nullptr;
    }
  }

  auto *reader = new handle{std::move(buffer), uncompressed, uint64_t{0}};
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

  const auto it = arc->index.find(name);
  if (it == arc->index.end()) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
    return 0;
  }

  const auto &entry = arc->entries[it->second];
  const auto directory = (entry.flags & FLAG_DIRECTORY) != 0;
  stat->filesize = directory ? -1 : static_cast<PHYSFS_sint64>(e.uncompressed);
  stat->modtime = -1;
  stat->createtime = -1;
  stat->accesstime = -1;
  stat->filetype = directory ? PHYSFS_FILETYPE_DIRECTORY : PHYSFS_FILETYPE_REGULAR;
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
