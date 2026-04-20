#include "cartridge.hpp"

namespace {
using namespace std::string_view_literals;

constexpr uint32_t CROM_MAGIC = 0x43524F4D;
constexpr uint32_t CROM_VERSION = 1;
constexpr uint8_t FLAG_DIRECTORY = 1;
constexpr uint64_t MAX_UNCOMPRESSED = 64 * 1024 * 1024;
constexpr uint32_t MAX_ENTRIES = 1u << 20;
constexpr uint64_t MAX_DIRECTORY_BYTES = 256 * 1024 * 1024;
constexpr uint32_t MAX_DICTIONARY_BYTES = 16 * 1024 * 1024;
constexpr size_t HEADER_SIZE = 24;
constexpr size_t METADATA_SIZE = 25;

using decoder_t = std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)>;

using dictionary_t = std::unique_ptr<ZSTD_DDict, decltype(&ZSTD_freeDDict)>;

struct entry final {
  std::string_view path;
  uint64_t offset;
  uint64_t compressed;
  uint64_t uncompressed;
  uint8_t flags;
};

struct archive final {
  PHYSFS_Io *io{nullptr};
  decoder_t decoder{nullptr, ZSTD_freeDCtx};
  dictionary_t dictionary{nullptr, ZSTD_freeDDict};
  PHYSFS_uint64 bytes{0};
  std::vector<char> paths;
  std::vector<uint8_t> compressed;
  std::vector<entry> entries;
  ankerl::unordered_dense::map<std::string_view, size_t> index;
  ankerl::unordered_dense::map<std::string_view, std::vector<const char *>> children;
};

struct handle final {
  std::shared_ptr<uint8_t[]> data;
  size_t size;
  uint64_t position;
};

template <std::integral Integer>
[[nodiscard]] Integer read(const uint8_t *pointer) noexcept {
  Integer value;
  std::memcpy(&value, pointer, sizeof(Integer));
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
  if (count > MAX_ENTRIES || directory_bytes > MAX_DIRECTORY_BYTES ||
      dictionary_bytes == 0 || dictionary_bytes > MAX_DICTIONARY_BYTES) [[unlikely]]
    return fail(PHYSFS_ERR_CORRUPT);

  std::vector<uint8_t> trained(dictionary_bytes);
  if (!drain(io, trained.data(), dictionary_bytes)) [[unlikely]] {
    io->destroy(io);
    return nullptr;
  }

  std::vector<uint8_t> toc(static_cast<size_t>(directory_bytes));
  if (directory_bytes > 0 && !drain(io, toc.data(), directory_bytes)) [[unlikely]] {
    io->destroy(io);
    return nullptr;
  }

  auto cartridge = std::make_unique<archive>();
  cartridge->io = io;
  cartridge->decoder.reset(ZSTD_createDCtx());
  cartridge->dictionary.reset(ZSTD_createDDict(trained.data(), dictionary_bytes));
  cartridge->entries.reserve(count);
  cartridge->index.reserve(count);
  cartridge->paths.reserve(static_cast<size_t>(directory_bytes));

  struct slot final {
    uint32_t offset;
    uint16_t length;
  };
  std::vector<slot> slots;
  slots.reserve(count);

  const auto *cursor = toc.data();
  const auto *end = cursor + toc.size();

  for (uint32_t index = 0; index < count; ++index) {
    if (static_cast<size_t>(end - cursor) < 2u) [[unlikely]]
      return fail(PHYSFS_ERR_CORRUPT);

    const auto length = read<uint16_t>(cursor);
    cursor += 2;

    if (length == 0 || static_cast<size_t>(end - cursor) < static_cast<size_t>(length) + METADATA_SIZE) [[unlikely]]
      return fail(PHYSFS_ERR_CORRUPT);

    slots.emplace_back(static_cast<uint32_t>(cartridge->paths.size()), length);
    cartridge->paths.insert(cartridge->paths.end(), cursor, cursor + length);
    cartridge->paths.emplace_back('\0');

    const auto *metadata = cursor + length;
    cartridge->entries.emplace_back(entry{
      {},
      read<uint64_t>(metadata),
      read<uint64_t>(metadata + 8),
      read<uint64_t>(metadata + 16),
      metadata[24],
    });

    cursor += static_cast<size_t>(length) + METADATA_SIZE;
  }

  // Bind path string_views now that cartridge->paths won't reallocate again.
  const auto *base = cartridge->paths.data();
  for (uint32_t index = 0; index < count; ++index) {
    auto &current = cartridge->entries[index];
    current.path = std::string_view{base + slots[index].offset, slots[index].length};

    cartridge->index.emplace(current.path, index);

    const auto slash = current.path.rfind('/');
    const auto parent = slash == std::string_view::npos ? ""sv : current.path.substr(0, slash);
    const auto leaf = slash == std::string_view::npos ? 0u : slash + 1;
    cartridge->children[parent].emplace_back(current.path.data() + leaf);
  }

  const auto total = io->length(io);
  cartridge->bytes = total < 0 ? 0 : static_cast<PHYSFS_uint64>(total);

  return cartridge.release();
}

PHYSFS_EnumerateCallbackResult crom_enumerate(void *opaque, const char *dirname, PHYSFS_EnumerateCallback callback, const char *origdir, void *cbdata) {
  auto *cartridge = static_cast<archive *>(opaque);

  const auto it = cartridge->children.find(dirname);
  if (it == cartridge->children.end())
    return PHYSFS_ENUM_OK;

  for (const auto *name : it->second) {
    const auto result = callback(cbdata, origdir, name);
    if (result != PHYSFS_ENUM_OK)
      return result;
  }

  return PHYSFS_ENUM_OK;
}

PHYSFS_Io *crom_open_read(void *opaque, const char *name) {
  auto *cartridge = static_cast<archive *>(opaque);

  const auto it = cartridge->index.find(name);
  if (it == cartridge->index.end()) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
    return nullptr;
  }

  const auto &found = cartridge->entries[it->second];

  if (found.flags & FLAG_DIRECTORY) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_A_FILE);
    return nullptr;
  }

  if (found.uncompressed > MAX_UNCOMPRESSED ||
      found.offset > cartridge->bytes ||
      found.compressed > cartridge->bytes - found.offset) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
    return nullptr;
  }

  const auto uncompressed = static_cast<size_t>(found.uncompressed);
  const auto compressed = static_cast<size_t>(found.compressed);

  auto buffer = std::make_shared_for_overwrite<uint8_t[]>(uncompressed);

  if (uncompressed > 0) [[likely]] {
    if (!cartridge->io->seek(cartridge->io, found.offset)) [[unlikely]] {
      PHYSFS_setErrorCode(PHYSFS_ERR_IO);
      return nullptr;
    }

    if (cartridge->compressed.size() < compressed)
      cartridge->compressed.resize(compressed);

    if (!drain(cartridge->io, cartridge->compressed.data(), compressed)) [[unlikely]] {
      PHYSFS_setErrorCode(PHYSFS_ERR_IO);
      return nullptr;
    }

    const auto result = ZSTD_decompress_usingDDict(
      cartridge->decoder.get(),
      buffer.get(), uncompressed,
      cartridge->compressed.data(), compressed,
      cartridge->dictionary.get()
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
  auto *cartridge = static_cast<archive *>(opaque);

  const auto it = cartridge->index.find(name);
  if (it == cartridge->index.end()) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
    return 0;
  }

  const auto &current = cartridge->entries[it->second];
  const auto directory = (current.flags & FLAG_DIRECTORY) != 0;
  stat->filesize = directory ? -1 : static_cast<PHYSFS_sint64>(current.uncompressed);
  stat->modtime = -1;
  stat->createtime = -1;
  stat->accesstime = -1;
  stat->filetype = directory ? PHYSFS_FILETYPE_DIRECTORY : PHYSFS_FILETYPE_REGULAR;
  stat->readonly = 1;

  return 1;
}

void crom_close_archive(void *opaque) {
  auto *cartridge = static_cast<archive *>(opaque);
  cartridge->io->destroy(cartridge->io);
  delete cartridge;
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
