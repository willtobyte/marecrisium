namespace {
constexpr uint8_t DIRECTORY = 1;
constexpr uint8_t UNCOMPRESSED = 2;
constexpr size_t HEADER = 16;
constexpr size_t RECORD = 32;

using decoder_t = std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)>;

using dictionary_t = std::unique_ptr<ZSTD_DDict, decltype(&ZSTD_freeDDict)>;

struct record final {
  uint64_t position;
  uint64_t compressed;
  uint64_t uncompressed;
  uint32_t path_offset;
  uint16_t path_length;
  uint8_t flags;
  uint8_t padding;
};

static_assert(sizeof(record) == RECORD);

struct archive final {
  PHYSFS_Io *source{nullptr};
  std::vector<uint8_t> manifest;
  decoder_t decoder{nullptr, ZSTD_freeDCtx};
  dictionary_t dictionary{nullptr, ZSTD_freeDDict};
  std::span<const record> records;
  size_t strings{};
};

struct backing final {
  const uint8_t *data;
  size_t size;
  uint32_t count;
};

struct handle final {
  PHYSFS_Io io;
  backing *shared;
  uint64_t position;
};

[[nodiscard]] inline std::string_view path_of(const archive *cartridge, const record &r) noexcept {
  return {reinterpret_cast<const char *>(cartridge->manifest.data() + cartridge->strings + r.path_offset), r.path_length};
}

template <std::integral Integer>
[[nodiscard]] Integer read(const uint8_t *pointer) noexcept {
  Integer value;
  std::memcpy(&value, pointer, sizeof(Integer));
  return value;
}

[[nodiscard]] size_t locate(const archive *cartridge, std::string_view name) noexcept {
  const auto &records = cartridge->records;
  const auto it = std::ranges::lower_bound(records, name, {}, [&](const record &r) { return path_of(cartridge, r); });
  if (it == records.end() || path_of(cartridge, *it) != name) [[unlikely]]
    return SIZE_MAX;

  return static_cast<size_t>(it - records.begin());
}

PHYSFS_sint64 bank_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 length) {
  auto *reader = static_cast<handle *>(io->opaque);
  auto *shared = reader->shared;
  const auto remaining = shared->size - reader->position;
  if (remaining == 0)
    return 0;

  const auto count = std::min(length, static_cast<PHYSFS_uint64>(remaining));
  std::memcpy(buffer, shared->data + reader->position, static_cast<size_t>(count));
  reader->position += count;
  return static_cast<PHYSFS_sint64>(count);
}

PHYSFS_sint64 bank_write(PHYSFS_Io *, const void *, PHYSFS_uint64) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return -1;
}

int bank_seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
  static_cast<handle *>(io->opaque)->position = offset;
  return 1;
}

PHYSFS_sint64 bank_tell(PHYSFS_Io *io) {
  return static_cast<PHYSFS_sint64>(static_cast<handle *>(io->opaque)->position);
}

PHYSFS_sint64 bank_length(PHYSFS_Io *io) {
  return static_cast<PHYSFS_sint64>(static_cast<handle *>(io->opaque)->shared->size);
}

PHYSFS_Io *bank_duplicate(PHYSFS_Io *io) {
  auto *reader = static_cast<handle *>(io->opaque);
  auto *copy = new handle{reader->io, reader->shared, uint64_t{0}};
  ++reader->shared->count;
  copy->io.opaque = copy;
  return &copy->io;
}

int bank_flush(PHYSFS_Io *) {
  return 1;
}

void bank_destroy(PHYSFS_Io *io) {
  auto *h = static_cast<handle *>(io->opaque);
  if (--h->shared->count == 0) {
    h->shared->~backing();
    ::operator delete(h->shared);
  }

  delete h;
}

void *crom_open_archive(PHYSFS_Io *io, const char *, int, int *claimed) {
  *claimed = 1;

  if (!io->seek(io, 0)) [[unlikely]]
    return nullptr;

  uint8_t header[HEADER];
  if (io->read(io, header, HEADER) != static_cast<PHYSFS_sint64>(HEADER)) [[unlikely]]
    return nullptr;

  const auto count = read<uint32_t>(header + 4);
  const auto stringsize = read<uint32_t>(header + 8);
  const auto trainsize = read<uint32_t>(header + 12);

  const auto size = HEADER + static_cast<size_t>(count) * RECORD + stringsize + trainsize;

  auto cartridge = std::make_unique<archive>();
  cartridge->manifest.resize(size);
  std::memcpy(cartridge->manifest.data(), header, HEADER);

  const auto remainder = size - HEADER;
  if (io->read(io, cartridge->manifest.data() + HEADER, remainder) != static_cast<PHYSFS_sint64>(remainder)) [[unlikely]]
    return nullptr;

  const auto *p = cartridge->manifest.data();
  assert(reinterpret_cast<uintptr_t>(p) % alignof(record) == 0);
  cartridge->records = std::span<const record>{reinterpret_cast<const record *>(p + HEADER), count};
  cartridge->strings = HEADER + static_cast<size_t>(count) * RECORD;

  cartridge->decoder.reset(ZSTD_createDCtx());
  cartridge->dictionary.reset(ZSTD_createDDict_byReference(p + cartridge->strings + stringsize, trainsize));
  assert(cartridge->decoder);
  assert(cartridge->dictionary);

  cartridge->source = io;
  return cartridge.release();
}

PHYSFS_EnumerateCallbackResult crom_enumerate(void *opaque, const char *dirname, PHYSFS_EnumerateCallback callback, const char *origdir, void *cbdata) {
  auto *cartridge = static_cast<archive *>(opaque);

  const std::string_view dir{dirname};
  static std::array<char, 512> buffer;
  if (dir.size() >= buffer.size()) [[unlikely]] return PHYSFS_ENUM_ERROR;
  auto *end = std::ranges::copy(dir, buffer.data()).out;
  if (dir.empty() || dir.back() != '/') [[likely]] *end++ = '/';
  const std::string_view needle{buffer.data(), end};

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
  assert((found.flags & DIRECTORY) == 0);

  const auto uncompressed = static_cast<size_t>(found.uncompressed);
  const auto compressed = static_cast<size_t>(found.compressed);

  const auto block = sizeof(backing) + uncompressed;
  auto *shared = static_cast<backing *>(::operator new(block));
  auto *tail = reinterpret_cast<uint8_t *>(shared + 1);
  new (shared) backing{tail, uncompressed, 1u};

  if (uncompressed > 0) [[likely]] {
    auto *source = cartridge->source;
    [[maybe_unused]] const auto seeked = source->seek(source, found.position);
    assert(seeked);

    if ((found.flags & UNCOMPRESSED) != 0) {
      [[maybe_unused]] const auto bytes = source->read(source, tail, uncompressed);
      assert(bytes == static_cast<PHYSFS_sint64>(uncompressed));
    } else {
      std::vector<uint8_t> scratch(compressed);
      [[maybe_unused]] const auto bytes = source->read(source, scratch.data(), compressed);
      assert(bytes == static_cast<PHYSFS_sint64>(compressed));

      [[maybe_unused]] const auto result = ZSTD_decompress_usingDDict(
        cartridge->decoder.get(),
        tail, uncompressed,
        scratch.data(), compressed,
        cartridge->dictionary.get());

      assert(result == uncompressed);
    }
  }

  auto *reader = new handle{
    PHYSFS_Io{
      .version = 0,
      .opaque = nullptr,
      .read = bank_read,
      .write = bank_write,
      .seek = bank_seek,
      .tell = bank_tell,
      .length = bank_length,
      .duplicate = bank_duplicate,
      .flush = bank_flush,
      .destroy = bank_destroy,
    },
    shared,
    uint64_t{0},
  };

  reader->io.opaque = reader;
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
  const auto directory = (current.flags & DIRECTORY) != 0;
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
  if (cartridge->source) [[likely]]
    cartridge->source->destroy(cartridge->source);

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
