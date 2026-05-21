namespace {
constexpr uint8_t DIRECTORY = 1;
constexpr uint8_t ALGO_RAW = 0;
constexpr uint8_t ALGO_ZSTD_DICT = 1;
constexpr size_t HEADER = 36;
constexpr size_t RECORD = 20;
constexpr uint32_t EMPTY = UINT32_MAX;
constexpr uint64_t PRIME = 0x9e3779b97f4a7c15ull;

using decoder_t = std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)>;

using dictionary_t = std::unique_ptr<ZSTD_DDict, decltype(&ZSTD_freeDDict)>;

struct record final {
  uint32_t position;
  uint32_t compressed;
  uint32_t uncompressed;
  uint32_t offset;
  uint16_t length;
  uint8_t flags;
  uint8_t algorithm;
};

static_assert(sizeof(record) == RECORD, "record layout must match on-disk size");

struct archive final {
  PHYSFS_Io *source{nullptr};
  std::vector<uint8_t> manifest;
  std::vector<uint32_t> storage;
  std::vector<uint8_t> strings;
  decoder_t decoder{nullptr, ZSTD_freeDCtx};
  dictionary_t dictionary{nullptr, ZSTD_freeDDict};
  std::span<const record> records;
  std::span<const uint32_t> buckets;
  uint32_t seed{};
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
  return {reinterpret_cast<const char *>(cartridge->strings.data() + r.offset), r.length};
}

[[nodiscard]] inline uint64_t hashfn(std::string_view name, uint64_t seed) noexcept {
  const auto *p = reinterpret_cast<const uint8_t *>(name.data());
  auto n = name.size();
  uint64_t h = seed ^ n;
  while (n >= 8) {
    uint64_t chunk;
    std::memcpy(&chunk, p, 8);
    h = mix(h ^ chunk, PRIME);
    p += 8;
    n -= 8;
  }

  if (n > 0) {
    uint64_t tail = 0;
    std::memcpy(&tail, p, n);
    h = mix(h ^ tail, PRIME);
  }

  return h;
}

[[nodiscard]] size_t locate(const archive *cartridge, std::string_view name) noexcept {
  const auto h = hashfn(name, cartridge->seed);
  const auto slot = static_cast<size_t>(h) & (cartridge->buckets.size() - 1);
  const auto index = cartridge->buckets[slot];
  if (index == EMPTY) [[unlikely]]
    return SIZE_MAX;

  return index;
}

PHYSFS_sint64 _read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 length) {
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

PHYSFS_sint64 _write(PHYSFS_Io *, const void *, PHYSFS_uint64) {
  PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
  return -1;
}

int _seek(PHYSFS_Io *io, PHYSFS_uint64 offset) {
  static_cast<handle *>(io->opaque)->position = offset;
  return 1;
}

PHYSFS_sint64 _tell(PHYSFS_Io *io) {
  return static_cast<PHYSFS_sint64>(static_cast<handle *>(io->opaque)->position);
}

PHYSFS_sint64 _length(PHYSFS_Io *io) {
  return static_cast<PHYSFS_sint64>(static_cast<handle *>(io->opaque)->shared->size);
}

PHYSFS_Io *_duplicate(PHYSFS_Io *io) {
  auto *reader = static_cast<handle *>(io->opaque);
  auto *copy = new handle{reader->io, reader->shared, uint64_t{0}};
  ++reader->shared->count;
  copy->io.opaque = copy;
  return &copy->io;
}

int _flush(PHYSFS_Io *) {
  return 1;
}

void _destroy(PHYSFS_Io *io) {
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

  alignas(uint64_t) uint8_t header[HEADER];
  if (io->read(io, header, HEADER) != static_cast<PHYSFS_sint64>(HEADER)) [[unlikely]]
    return nullptr;

  const auto *fields = reinterpret_cast<const uint32_t *>(header);
  const auto count = fields[1];
  const auto stringsize = fields[2];
  const auto strings = fields[3];
  const auto trainsize = fields[4];
  const auto slots = fields[5];
  const auto seed = fields[6];
  const auto buckets = fields[7];

  const auto size = HEADER + static_cast<size_t>(count) * RECORD + buckets + strings + trainsize;

  auto cartridge = std::make_unique<archive>();
  cartridge->manifest.resize(size);
  std::memcpy(cartridge->manifest.data(), header, HEADER);

  const auto remainder = size - HEADER;
  if (io->read(io, cartridge->manifest.data() + HEADER, remainder) != static_cast<PHYSFS_sint64>(remainder)) [[unlikely]]
    return nullptr;

  const auto *p = cartridge->manifest.data();
  [[assume(reinterpret_cast<uintptr_t>(p) % alignof(record) == 0)]];
  cartridge->records = std::span<const record>{reinterpret_cast<const record *>(p + HEADER), count};
  auto cursor = HEADER + static_cast<size_t>(count) * RECORD;
  cartridge->seed = seed;

  cartridge->storage.resize(slots);
  const auto bytes = ZSTD_decompress(
    cartridge->storage.data(), static_cast<size_t>(slots) * sizeof(uint32_t), p + cursor, buckets);
  [[assume(bytes == static_cast<size_t>(slots) * sizeof(uint32_t))]];
  cartridge->buckets = std::span<const uint32_t>{cartridge->storage};
  cursor += buckets;

  cartridge->strings.resize(stringsize);
  const auto written = ZSTD_decompress(
    cartridge->strings.data(), stringsize, p + cursor, strings);
  [[assume(written == stringsize)]];
  cursor += strings;

  cartridge->decoder.reset(ZSTD_createDCtx());
  cartridge->dictionary.reset(ZSTD_createDDict_byReference(p + cursor, trainsize));

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
  [[assume((found.flags & DIRECTORY) == 0)]];

  const auto uncompressed = static_cast<size_t>(found.uncompressed);
  const auto compressed = static_cast<size_t>(found.compressed);
  const auto block = sizeof(backing) + uncompressed;

  auto *storage = static_cast<backing *>(::operator new(block));
  auto *tail = reinterpret_cast<uint8_t *>(storage + 1);
  new (storage) backing{tail, uncompressed, 1u};

  if (uncompressed > 0) [[likely]] {
    auto *source = cartridge->source;
    const auto seeked = source->seek(source, found.position);
    [[assume(seeked != 0)]];

    switch (found.algorithm) {
      case ALGO_RAW: {
        const auto bytes = source->read(source, tail, uncompressed);
        [[assume(bytes == static_cast<PHYSFS_sint64>(uncompressed))]];
        break;
      }

      case ALGO_ZSTD_DICT: {
        std::vector<uint8_t> scratch(compressed);
        const auto bytes = source->read(source, scratch.data(), compressed);
        [[assume(bytes == static_cast<PHYSFS_sint64>(compressed))]];

        const auto result = ZSTD_decompress_usingDDict(
          cartridge->decoder.get(),
          tail, uncompressed,
          scratch.data(), compressed,
          cartridge->dictionary.get());

        [[assume(result == uncompressed)]];
        break;
      }

      default:
        std::unreachable();
    }
  }

  auto *reader = new handle{
    PHYSFS_Io{
      .version = 0,
      .opaque = nullptr,
      .read = _read,
      .write = _write,
      .seek = _seek,
      .tell = _tell,
      .length = _length,
      .duplicate = _duplicate,
      .flush = _flush,
      .destroy = _destroy,
    },
    storage,
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
