namespace {
constexpr uint8_t FLAG_DIRECTORY = 1;
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
  std::vector<uint8_t> strings;
  std::vector<record> records;
  std::vector<uint8_t> compressed;
};

struct handle final {
  PHYSFS_Io io;
  size_t size;
  uint64_t position;
};

[[nodiscard]] inline uint8_t *tail(handle *h) noexcept {
  return reinterpret_cast<uint8_t *>(h + 1);
}

[[nodiscard]] inline std::string_view path_of(const archive *cartridge, const record &r) noexcept {
  return {reinterpret_cast<const char *>(cartridge->strings.data() + r.path_offset), r.path_length};
}

template <std::integral Integer>
[[nodiscard]] Integer read(const uint8_t *pointer) noexcept {
  Integer value;
  std::memcpy(&value, pointer, sizeof(Integer));
  return value;
}

void slurp(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 length) noexcept {
  const auto got = io->read(io, buffer, length);
  assert(got == static_cast<PHYSFS_sint64>(length));
}

[[nodiscard]] size_t locate(const archive *cartridge, std::string_view name) noexcept {
  const auto &records = cartridge->records;
  const auto it = std::ranges::lower_bound(records, name, {}, [&](const record &r) { return path_of(cartridge, r); });
  if (it == records.end() || path_of(cartridge, *it) != name) [[unlikely]]
    return SIZE_MAX;

  return static_cast<size_t>(it - records.begin());
}

PHYSFS_sint64 file_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 length) {
  auto *reader = static_cast<handle *>(io->opaque);
  const auto remaining = reader->size - reader->position;
  if (remaining == 0)
    return 0;

  const auto count = std::min(length, static_cast<PHYSFS_uint64>(remaining));
  std::memcpy(buffer, tail(reader) + reader->position, static_cast<size_t>(count));
  reader->position += count;
  return static_cast<PHYSFS_sint64>(count);
}

PHYSFS_sint64 file_write(PHYSFS_Io *, const void *, PHYSFS_uint64) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return -1;
}

int file_seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
  static_cast<handle *>(io->opaque)->position = offset;
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
  auto *copy = static_cast<handle *>(::operator new(sizeof(handle) + reader->size));
  new (copy) handle{reader->io, reader->size, uint64_t{0}};
  copy->io.opaque = copy;
  std::memcpy(tail(copy), tail(reader), reader->size);
  return &copy->io;
}

int file_flush(PHYSFS_Io *) {
  return 1;
}

void file_destroy(PHYSFS_Io *io) {
  auto *h = static_cast<handle *>(io->opaque);
  h->~handle();
  ::operator delete(h);
}

void *crom_open_archive(PHYSFS_Io *io, const char *, int, int *claimed) {
  uint8_t header[HEADER_SIZE];
  assert(io->seek(io, 0));
  slurp(io, header, sizeof(header));

  *claimed = 1;

  const auto count = read<uint32_t>(header + 4);
  const auto string_bytes = read<uint32_t>(header + 8);
  const auto dictionary_bytes = read<uint32_t>(header + 12);

  auto cartridge = std::make_unique<archive>();
  cartridge->io = io;

  std::vector<uint8_t> trained(dictionary_bytes);
  slurp(io, trained.data(), dictionary_bytes);

  cartridge->strings.resize(string_bytes);
  if (string_bytes > 0)
    slurp(io, cartridge->strings.data(), string_bytes);

  cartridge->records.resize(count);
  if (count > 0)
    slurp(io, cartridge->records.data(), static_cast<PHYSFS_uint64>(count) * RECORD_SIZE);

  cartridge->decoder.reset(ZSTD_createDCtx());
  cartridge->dictionary.reset(ZSTD_createDDict(trained.data(), dictionary_bytes));
  assert(cartridge->decoder);
  assert(cartridge->dictionary);

  return cartridge.release();
}

PHYSFS_EnumerateCallbackResult crom_enumerate(void *opaque, const char *dirname, PHYSFS_EnumerateCallback callback, const char *origdir, void *cbdata) {
  auto *cartridge = static_cast<archive *>(opaque);

  const std::string_view raw{dirname};
  char buffer[512];
  if (raw.size() + 1 >= sizeof(buffer)) [[unlikely]] return PHYSFS_ENUM_ERROR;
  auto end = std::ranges::copy(raw, buffer).out;
  if (raw.empty() || raw.back() != '/') [[likely]] *end++ = '/';
  const std::string_view needle{buffer, end};

  const auto &records = cartridge->records;
  auto it = std::ranges::lower_bound(records, needle, {}, [&](const record &r) { return path_of(cartridge, r); });

  for (; it != records.end(); ++it) {
    const auto path = path_of(cartridge, *it);
    if (!path.starts_with(needle)) [[unlikely]]
      break;

    const auto leaf = path.substr(needle.size());
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
  assert((found.flags & FLAG_DIRECTORY) == 0);

  const auto uncompressed = static_cast<size_t>(found.uncompressed);
  const auto compressed = static_cast<size_t>(found.compressed);

  auto *reader = static_cast<handle *>(::operator new(sizeof(handle) + uncompressed));
  new (reader) handle{
    PHYSFS_Io{
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
    },
    uncompressed,
    uint64_t{0},
  };

  if (uncompressed > 0) [[likely]] {
    assert(cartridge->io->seek(cartridge->io, found.data_offset));

    if (cartridge->compressed.size() < compressed)
      cartridge->compressed.resize(compressed);

    slurp(cartridge->io, cartridge->compressed.data(), compressed);

    const auto result = ZSTD_decompress_usingDDict(
      cartridge->decoder.get(),
      tail(reader), uncompressed,
      cartridge->compressed.data(), compressed,
      cartridge->dictionary.get()
    );

    assert(result == uncompressed);
  }

  return &reader->io;
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
