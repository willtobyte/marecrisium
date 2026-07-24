namespace {
constexpr uint8_t directory = 1;
constexpr uint8_t raw = 0;
constexpr uint8_t zstd = 1;
constexpr size_t header = 36;
constexpr size_t stride = 20;
constexpr uint32_t empty = UINT32_MAX;
constexpr uint64_t prime = 0x9e3779b97f4a7c15ull;

struct zstd_deleter final {
  void operator()(ZSTD_DCtx *context) const noexcept { ZSTD_freeDCtx(context); }
  void operator()(ZSTD_DDict *dictionary) const noexcept { ZSTD_freeDDict(dictionary); }
};

using decoder_t = std::unique_ptr<ZSTD_DCtx, zstd_deleter>;
using dictionary_t = std::unique_ptr<ZSTD_DDict, zstd_deleter>;

struct record final {
  uint32_t position;
  uint32_t compressed;
  uint32_t uncompressed;
  uint32_t offset;
  uint16_t length;
  uint8_t flags;
  uint8_t algorithm;
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
  std::unique_ptr<uint32_t[]> storage;
  std::unique_ptr<uint8_t[]> strings;
  decoder_t decoder;
  dictionary_t dictionary;
  std::span<const record> records;
  std::span<const uint32_t> buckets;
  uint32_t seed;

  explicit archive(std::string_view filename)
    : source{filename} {
    const auto *fields = reinterpret_cast<const uint32_t *>(source.data);
    [[assume(reinterpret_cast<uintptr_t>(fields) % alignof(uint32_t) == 0)]];

    const auto count = fields[1];
    const auto textsize = fields[2];
    const auto text = fields[3];
    const auto trainsize = fields[4];
    const auto slots = fields[5];
    seed = fields[6];
    const auto table = fields[7];

    const auto *data = source.data;
    records = {reinterpret_cast<const record *>(data + header), count};
    auto cursor = header + static_cast<size_t>(count) * stride;

    storage = std::make_unique_for_overwrite<uint32_t[]>(slots);
    const auto bytes = ZSTD_decompress(
      storage.get(), static_cast<size_t>(slots) * sizeof(uint32_t), data + cursor, table);
    [[assume(bytes == static_cast<size_t>(slots) * sizeof(uint32_t))]];
    buckets = {storage.get(), slots};
    cursor += table;

    strings = std::make_unique_for_overwrite<uint8_t[]>(textsize);
    const auto written = ZSTD_decompress(strings.get(), textsize, data + cursor, text);
    [[assume(written == textsize)]];
    cursor += text;

    decoder.reset(ZSTD_createDCtx());
    dictionary.reset(ZSTD_createDDict_byReference(data + cursor, trainsize));
  }
};

std::unique_ptr<archive> content;

[[nodiscard]] inline std::string_view path_of(const archive *cartridge, const record &current) noexcept {
  return {reinterpret_cast<const char *>(cartridge->strings.get() + current.offset), current.length};
}

[[nodiscard]] inline uint64_t hashfn(std::string_view name, uint64_t seed) noexcept {
  const auto *p = reinterpret_cast<const uint8_t *>(name.data());
  auto n = name.size();
  uint64_t h = seed ^ n;
  while (n >= 8) {
    uint64_t chunk;
    std::memcpy(&chunk, p, 8);
    h = mix(h ^ chunk, prime);
    p += 8;
    n -= 8;
  }

  const auto rem = n;
  uint64_t tail = 0;
  for (unsigned shift = 0; n > 0; shift += 8, --n)
    tail |= static_cast<uint64_t>(*p++) << shift;

  return rem == 0 ? h : mix(h ^ tail, prime);
}

[[nodiscard]] size_t locate(const archive *cartridge, std::string_view name) noexcept {
  const auto h = hashfn(name, cartridge->seed);
  const auto slot = static_cast<size_t>(h) & (cartridge->buckets.size() - 1);
  const auto index = cartridge->buckets[slot];
  if (index == empty || path_of(cartridge, cartridge->records[index]) != name) [[unlikely]]
    return SIZE_MAX;

  return index;
}
}

void io::mount(std::string_view filename) {
  assert(!content && "cartridge already mounted");
  content = std::make_unique<archive>(filename);
}

bool io::exists(std::string_view filename) noexcept {
  const auto *cartridge = content.get();
  [[assume(cartridge != nullptr)]];
  return locate(cartridge, filename) != SIZE_MAX;
}

bytes io::read(std::string_view filename) {
  auto *cartridge = content.get();
  [[assume(cartridge != nullptr)]];
  const auto index = locate(cartridge, filename);
  if (index == SIZE_MAX) [[unlikely]]
    throw std::runtime_error{std::format("[io::read] file not found: {}", filename)};

  const auto &current = cartridge->records[index];
  [[assume((current.flags & directory) == 0)]];
  const auto size = static_cast<size_t>(current.uncompressed);
  bytes buffer(size);
  if (size == 0) [[unlikely]]
    return buffer;

  const auto *source = cartridge->source.data + current.position;
  if (current.algorithm == raw) {
    std::memcpy(buffer.data(), source, size);
    return buffer;
  }

  [[assume(current.algorithm == zstd)]];
  const auto result = ZSTD_decompress_usingDDict(
    cartridge->decoder.get(),
    buffer.data(), size,
    source, current.compressed,
    cartridge->dictionary.get());
  [[assume(result == size)]];
  return buffer;
}
