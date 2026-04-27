namespace {
constexpr uint32_t CROM_MAGIC = 0x43524F4D;
constexpr uint8_t FLAG_DIRECTORY = 1;
constexpr uint64_t MAX_UNCOMPRESSED = 64 * 1024 * 1024;
constexpr uint32_t MAX_ENTRIES = 1u << 20;
constexpr uint32_t MAX_DIRECTORY_BYTES = 256 * 1024 * 1024;
constexpr uint32_t MAX_DICTIONARY_BYTES = 16 * 1024 * 1024;
constexpr size_t HEADER_SIZE = 16;
constexpr size_t RECORD_SIZE = 32;

using decoder_t = std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)>;

using dictionary_t = std::unique_ptr<ZSTD_DDict, decltype(&ZSTD_freeDDict)>;

struct record final {
  uint64_t data_offset;
  uint64_t compressed;
  uint64_t uncompressed;
  uint32_t path_offset;
  uint16_t path_length;
  uint8_t flags;
  uint8_t padding;
};

static_assert(sizeof(record) == RECORD_SIZE);

struct archive final {
  PHYSFS_Io *io{nullptr};
  decoder_t decoder{nullptr, ZSTD_freeDCtx};
  dictionary_t dictionary{nullptr, ZSTD_freeDDict};
  PHYSFS_uint64 bytes{0};
  std::vector<uint8_t> strings;
  std::vector<record> records;
  std::vector<std::string_view> paths;
  std::vector<uint8_t> compressed;
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

[[nodiscard]] size_t locate(const archive *cartridge, std::string_view name) noexcept {
  const auto &paths = cartridge->paths;
  const auto it = std::ranges::lower_bound(paths, name);
  if (it == paths.end() || *it != name) [[unlikely]]
    return SIZE_MAX;

  return static_cast<size_t>(it - paths.begin());
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

  const auto count = read<uint32_t>(header + 4);
  const auto string_bytes = read<uint32_t>(header + 8);
  const auto dictionary_bytes = read<uint32_t>(header + 12);

  if (count > MAX_ENTRIES ||
      string_bytes > MAX_DIRECTORY_BYTES ||
      dictionary_bytes == 0 ||
      dictionary_bytes > MAX_DICTIONARY_BYTES) [[unlikely]]
    return fail(PHYSFS_ERR_CORRUPT);

  auto cartridge = std::make_unique<archive>();
  cartridge->io = io;

  std::vector<uint8_t> trained(dictionary_bytes);
  if (!drain(io, trained.data(), dictionary_bytes)) [[unlikely]]
    return fail(PHYSFS_ERR_IO);

  cartridge->strings.resize(string_bytes);
  if (string_bytes > 0 && !drain(io, cartridge->strings.data(), string_bytes)) [[unlikely]]
    return fail(PHYSFS_ERR_IO);

  cartridge->records.resize(count);
  if (count > 0 && !drain(io, cartridge->records.data(),
                          static_cast<PHYSFS_uint64>(count) * RECORD_SIZE)) [[unlikely]]
    return fail(PHYSFS_ERR_IO);

  cartridge->decoder.reset(ZSTD_createDCtx());
  cartridge->dictionary.reset(ZSTD_createDDict(trained.data(), dictionary_bytes));

  cartridge->paths.reserve(count);
  const auto *base = cartridge->strings.data();
  for (const auto &current : cartridge->records) {
    if (static_cast<uint64_t>(current.path_offset) + current.path_length > string_bytes) [[unlikely]]
      return fail(PHYSFS_ERR_CORRUPT);

    cartridge->paths.emplace_back(
      reinterpret_cast<const char *>(base + current.path_offset),
      current.path_length);
  }

  const auto total = io->length(io);
  cartridge->bytes = total < 0 ? 0 : static_cast<PHYSFS_uint64>(total);

  return cartridge.release();
}

PHYSFS_EnumerateCallbackResult crom_enumerate(void *opaque, const char *dirname, PHYSFS_EnumerateCallback callback, const char *origdir, void *cbdata) {
  auto *cartridge = static_cast<archive *>(opaque);

  std::string prefix = dirname;
  if (!prefix.empty() && prefix.back() != '/') [[likely]]
    prefix.push_back('/');

  const auto &paths = cartridge->paths;
  const std::string_view needle{prefix};
  auto it = std::ranges::lower_bound(paths, needle);

  for (; it != paths.end(); ++it) {
    if (!it->starts_with(needle)) [[unlikely]]
      break;

    const auto leaf = it->substr(needle.size());
    if (leaf.empty() || leaf.find('/') != std::string_view::npos) [[unlikely]]
      continue;

    char name[512];
    if (leaf.size() >= sizeof(name)) [[unlikely]]
      continue;
    std::memcpy(name, leaf.data(), leaf.size());
    name[leaf.size()] = '\0';

    const auto result = callback(cbdata, origdir, name);
    if (result != PHYSFS_ENUM_OK) [[unlikely]]
      return result;
  }

  return PHYSFS_ENUM_OK;
}

PHYSFS_Io *crom_open_read(void *opaque, const char *name) {
  auto *cartridge = static_cast<archive *>(opaque);

  const auto index = locate(cartridge, name);
  if (index == SIZE_MAX) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
    return nullptr;
  }

  const auto &found = cartridge->records[index];

  if (found.flags & FLAG_DIRECTORY) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_A_FILE);
    return nullptr;
  }

  if (found.uncompressed > MAX_UNCOMPRESSED ||
      found.data_offset > cartridge->bytes ||
      found.compressed > cartridge->bytes - found.data_offset) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
    return nullptr;
  }

  const auto uncompressed = static_cast<size_t>(found.uncompressed);
  const auto compressed = static_cast<size_t>(found.compressed);

  auto buffer = std::make_shared_for_overwrite<uint8_t[]>(uncompressed);

  if (uncompressed > 0) [[likely]] {
    if (!cartridge->io->seek(cartridge->io, found.data_offset)) [[unlikely]] {
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

  const auto index = locate(cartridge, name);
  if (index == SIZE_MAX) [[unlikely]] {
    PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
    return 0;
  }

  const auto &current = cartridge->records[index];
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
