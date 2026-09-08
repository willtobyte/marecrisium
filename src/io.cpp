namespace {
constexpr uint8_t stored = 0;
constexpr size_t header = 32;
constexpr size_t stride = 32;
constexpr uint64_t prime = 0x9e3779b97f4a7c15ull;

using decoder_t = std::unique_ptr<ZSTD_DCtx, ZSTD_DCtx_Deleter>;
using dictionary_t = std::unique_ptr<ZSTD_DDict, ZSTD_DDict_Deleter>;

struct record final {
  uint64_t digest;
  uint32_t position;
  uint32_t compressed;
  uint32_t uncompressed;
  uint32_t offset;
  uint8_t length;
  uint8_t kind;
};

static_assert(sizeof(record) == stride, "record stride must match on-disk size");
static_assert(offsetof(record, digest) == 0, "record digest must lead for probing loads");

struct mapping final {
  const uint8_t *data{};
  size_t size{};

#ifdef _WIN32
  HANDLE file{INVALID_HANDLE_VALUE};
  HANDLE map{};
#else
  int file{-1};
#endif

  explicit mapping(std::string_view filename) {
    const std::filesystem::path path{filename};
#ifdef _WIN32
    file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) [[unlikely]]
      throw std::runtime_error{std::format("[io::mount] failed to open {}: {}", filename, GetLastError())};

    LARGE_INTEGER bytes;
    if (!GetFileSizeEx(file, &bytes)) [[unlikely]] {
      const auto error = GetLastError();
      CloseHandle(file);
      throw std::runtime_error{std::format("[io::mount] failed to size {}: {}", filename, error)};
    }

    size = static_cast<size_t>(bytes.QuadPart);
    map = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!map) [[unlikely]] {
      const auto error = GetLastError();
      CloseHandle(file);
      throw std::runtime_error{std::format("[io::mount] failed to map {}: {}", filename, error)};
    }

    const auto address = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
    if (!address) [[unlikely]] {
      const auto error = GetLastError();
      CloseHandle(map);
      CloseHandle(file);
      throw std::runtime_error{std::format("[io::mount] failed to view {}: {}", filename, error)};
    }

    data = static_cast<const uint8_t *>(address);
#else
    file = ::open(path.c_str(), O_RDONLY);
    if (file == -1) [[unlikely]]
      throw std::runtime_error{std::format("[io::mount] failed to open {}", filename)};

    struct stat info;
    if (fstat(file, &info) == -1) [[unlikely]] {
      close(file);
      throw std::runtime_error{std::format("[io::mount] failed to size {}", filename)};
    }

    size = static_cast<size_t>(info.st_size);
    const auto address = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, file, 0);
    if (address == MAP_FAILED) [[unlikely]] {
      close(file);
      throw std::runtime_error{std::format("[io::mount] failed to map {}", filename)};
    }

    data = static_cast<const uint8_t *>(address);
#endif
  }

  ~mapping() {
#ifdef _WIN32
    UnmapViewOfFile(data);
    CloseHandle(map);
    CloseHandle(file);
#else
    munmap(const_cast<uint8_t *>(data), size);
    close(file);
#endif
  }

  mapping(const mapping &) = delete;
  mapping &operator=(const mapping &) = delete;
};

struct archive final {
  mapping source;
  decoder_t decoder;
  dictionary_t dictionary;
  std::unique_ptr<uint8_t[]> arena;
  const uint8_t *records{};
  const uint8_t *strings{};
  size_t mask{};

  explicit archive(std::string_view filename)
      : source{filename} {
    std::array<uint32_t, 6> fields;
    std::memcpy(fields.data(), source.data, sizeof(fields));

    const auto textsize = fields[2];
    const auto trainsize = fields[3];
    const auto slots = fields[4];
    const auto arenasize = fields[5];
    mask = (slots >> 1) - 1;

    const auto *data = source.data;
    records = data + header;
    strings = records + static_cast<size_t>(slots) * stride;
    const auto *trained = strings + textsize;

    arena = std::make_unique_for_overwrite<uint8_t[]>(arenasize);
    decoder.reset(ZSTD_createDCtx());
    dictionary.reset(ZSTD_createDDict_byReference(trained, trainsize));
  }
};

std::optional<archive> content;

[[nodiscard]] inline uint64_t hashfn(std::string_view name) noexcept {
  const auto *cursor = reinterpret_cast<const uint8_t *>(name.data());

  auto remaining = name.size();
  uint64_t digest = remaining;

  while (remaining >= 8) {
    uint64_t chunk;
    std::memcpy(&chunk, cursor, 8);
    digest = mix(digest ^ chunk, prime);
    cursor += 8;
    remaining -= 8;
  }

  const auto tail = remaining;
  uint64_t value = 0;
  for (unsigned shift = 0; remaining > 0; shift += 8, --remaining)
    value |= static_cast<uint64_t>(*cursor++) << shift;

  return tail == 0 ? digest : mix(digest ^ value, prime);
}

[[nodiscard]] std::span<const uint8_t> decode(archive *cartridge, const record& current) {
  const auto size = static_cast<size_t>(current.uncompressed);

  const auto *source = cartridge->source.data + current.position;
  if (current.kind == stored)
    return {source, size};

  auto *buffer = cartridge->arena.get();
  ZSTD_decompress_usingDDict(
    cartridge->decoder.get(),
    buffer, size,
    source, current.compressed,
    cartridge->dictionary.get());

  return {buffer, size};
}
}

void io::mount(std::string_view filename) {
  content.emplace(filename);
}

std::optional<std::span<const uint8_t>> io::try_read(std::string_view filename) {
  auto *cartridge = &*content;

  const auto digest = hashfn(filename);
  const auto first = static_cast<size_t>(digest) & cartridge->mask;
  const auto *address = cartridge->records + first * stride;

  uint64_t candidate;
  std::memcpy(&candidate, address, sizeof(candidate));

  if (candidate != digest) [[unlikely]] {
    if (candidate == 0)
      return std::nullopt;

    const auto second = cartridge->mask + 1 + ((digest >> 32) & cartridge->mask);
    address = cartridge->records + second * stride;
    std::memcpy(&candidate, address, sizeof(candidate));
  }

  if (candidate != digest || digest == 0) [[unlikely]]
    return std::nullopt;

  record current;
  std::memcpy(&current, address, stride);

  return decode(cartridge, current);
}

std::span<const uint8_t> io::read(std::string_view filename) {
  auto *cartridge = &*content;

  const auto digest = hashfn(filename);
  const auto first = static_cast<size_t>(digest) & cartridge->mask;

  record current;
  std::memcpy(&current, cartridge->records + first * stride, stride);
  if (current.digest != digest) [[unlikely]] {
    const auto second = cartridge->mask + 1 + ((digest >> 32) & cartridge->mask);
    std::memcpy(&current, cartridge->records + second * stride, stride);
  }

  return decode(cartridge, current);
}
