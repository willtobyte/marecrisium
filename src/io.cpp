namespace {
constexpr uint8_t raw = 0;
constexpr uint8_t zstd = 1;
constexpr uint8_t directory = 2;
constexpr size_t header = 64;
constexpr size_t stride = 16;
constexpr uint16_t empty = UINT16_MAX;
constexpr uint64_t prime = 0x9e3779b97f4a7c15ull;

using decoder_t = std::unique_ptr<ZSTD_DCtx, ZSTD_DCtx_Deleter>;
using dictionary_t = std::unique_ptr<ZSTD_DDict, ZSTD_DDict_Deleter>;

struct record final {
  uint32_t position;
  uint32_t compressed;
  uint32_t uncompressed;
  uint16_t offset;
  uint8_t length;
  uint8_t kind;
};

static_assert(sizeof(record) == stride, "record stride must match on-disk size");

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
  const uint8_t *records{};
  const uint8_t *buckets{};
  const uint8_t *strings{};
  size_t mask{};
  uint32_t seed{};

  explicit archive(std::string_view filename)
    : source{filename} {
    std::array<uint32_t, 6> fields;
    std::memcpy(fields.data(), source.data, sizeof(fields));

    const auto count = fields[1];
    const auto textsize = fields[2];
    const auto trainsize = fields[3];
    const auto slots = fields[4];
    mask = slots - 1;
    seed = fields[5];

    const auto *data = source.data;
    buckets = data + header;
    records = buckets + static_cast<size_t>(slots) * sizeof(uint16_t);
    strings = records + static_cast<size_t>(count) * stride;
    const auto *trained = strings + textsize;

    decoder.reset(ZSTD_createDCtx());
    dictionary.reset(ZSTD_createDDict_byReference(trained, trainsize));
  }
};

std::optional<archive> content;

[[nodiscard]] inline record record_at(const archive *cartridge, size_t index) noexcept {
  record current;
  std::memcpy(&current, cartridge->records + index * stride, stride);
  return current;
}

[[nodiscard]] inline std::string_view path_of(const archive *cartridge, const record &current) noexcept {
  return {reinterpret_cast<const char *>(cartridge->strings + current.offset), current.length};
}

[[nodiscard]] inline uint64_t hashfn(std::string_view name, uint64_t seed) noexcept {
  const auto *cursor = reinterpret_cast<const uint8_t *>(name.data());
  auto remaining = name.size();
  uint64_t hash = seed ^ remaining;
  while (remaining >= 8) {
    uint64_t chunk;
    std::memcpy(&chunk, cursor, 8);
    hash = mix(hash ^ chunk, prime);
    cursor += 8;
    remaining -= 8;
  }

  const auto rem = remaining;
  uint64_t tail = 0;
  for (unsigned shift = 0; remaining > 0; shift += 8, --remaining)
    tail |= static_cast<uint64_t>(*cursor++) << shift;

  return rem == 0 ? hash : mix(hash ^ tail, prime);
}

[[nodiscard]] size_t locate(const archive *cartridge, std::string_view name) noexcept {
  const auto hash = hashfn(name, cartridge->seed);
  const auto slot = static_cast<size_t>(hash) & cartridge->mask;
  uint16_t index;
  std::memcpy(&index, cartridge->buckets + slot * sizeof(index), sizeof(index));
  if (index == empty) [[unlikely]]
    return SIZE_MAX;

  const auto current = record_at(cartridge, index);
  return path_of(cartridge, current) == name ? index : SIZE_MAX;
}
}

void io::mount(std::string_view filename) {
  assert(!content && "cartridge already mounted");
  content.emplace(filename);
}

bool io::exists(std::string_view filename) noexcept {
  const auto mounted = content.has_value();
  assert(mounted && "cartridge must be mounted");
  [[assume(mounted)]];
  return locate(&*content, filename) != SIZE_MAX;
}

bytes io::read(std::string_view filename) {
  const auto mounted = content.has_value();
  assert(mounted && "cartridge must be mounted");
  [[assume(mounted)]];
  auto *cartridge = &*content;
  const auto index = locate(cartridge, filename);
  if (index == SIZE_MAX) [[unlikely]]
    throw std::runtime_error{std::format("[io::read] file not found: {}", filename)};

  const auto current = record_at(cartridge, index);
  assert(current.kind != directory && "cartridge entry must be a file");
  [[assume(current.kind != directory)]];
  const auto size = static_cast<size_t>(current.uncompressed);
  if (size == 0) [[unlikely]]
    return {};

  const auto *source = cartridge->source.data + current.position;
  if (current.kind == raw)
    return {source, size};

  assert(current.kind == zstd && "cartridge compression must be supported");
  [[assume(current.kind == zstd)]];
  bytes buffer(size);
  const auto result = ZSTD_decompress_usingDDict(
    cartridge->decoder.get(),
    buffer.writable(), size,
    source, current.compressed,
    cartridge->dictionary.get());
  assert(result == size && "cartridge payload size must be valid");
  [[assume(result == size)]];
  return buffer;
}
