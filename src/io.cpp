blob::blob() noexcept
  : buffer{}, length{} {}

blob::blob(std::size_t length)
  : buffer{std::make_unique_for_overwrite<uint8_t[]>(length)}, length{length} {}

blob::blob(std::unique_ptr<backing, releaser> storage) noexcept
  : storage{std::move(storage)}, length{STORED} {}

blob::blob(blob &&other) noexcept
  : length{other.length} {
  if (other.stored()) {
    std::construct_at(&storage, std::move(other.storage));
    std::destroy_at(&other.storage);
    std::construct_at(&other.buffer);
  } else {
    std::construct_at(&buffer, std::move(other.buffer));
  }

  other.length = 0;
}

blob &blob::operator=(blob &&other) noexcept {
  if (this == &other)
    return *this;

  this->~blob();
  std::construct_at(this, std::move(other));
  return *this;
}

blob::~blob() {
  if (stored())
    std::destroy_at(&storage);
  else
    std::destroy_at(&buffer);
}

uint8_t *blob::data() noexcept {
  return stored() ? storage->data() : buffer.get();
}

const uint8_t *blob::data() const noexcept {
  return stored() ? storage->data() : buffer.get();
}

std::size_t blob::size() const noexcept {
  return stored() ? storage->length : length;
}

blob::operator std::span<const uint8_t>() const noexcept {
  return {data(), size()};
}

blob::operator bool() const noexcept {
  return stored() ? static_cast<bool>(storage) : static_cast<bool>(buffer);
}

bool io::exists(std::string_view filename) {
  return PHYSFS_exists(filename.data());
}

blob io::read(std::string_view filename) {
  capture capture;
  const auto file = std::unique_ptr<PHYSFS_File, PHYSFS_Deleter>{PHYSFS_openRead(filename.data())};
  if (!file) [[unlikely]]
    throw std::runtime_error{std::format("[PHYSFS_openRead] error while opening file: {}", filename)};

  if (auto buffer = capture.finish(); buffer) [[likely]]
    return buffer;

  const auto bytes = PHYSFS_fileLength(file.get());
  assert(bytes >= 0 && "[PHYSFS_fileLength] unknown length");
  const auto length = static_cast<std::size_t>(bytes);
  blob buffer{length};
  [[maybe_unused]] const auto result = PHYSFS_readBytes(file.get(), buffer.data(), length);
  assert(result == static_cast<PHYSFS_sint64>(length) && "[PHYSFS_readBytes] failed to read expected number of bytes");

  return buffer;
}

std::vector<std::string> io::enumerate(std::string_view directory) {
  std::unique_ptr<char*[], PHYSFS_Deleter> ptr{PHYSFS_enumerateFiles(directory.data())};
  assert(ptr && "[PHYSFS_enumerateFiles] failed to enumerate directory");

  auto **data = ptr.get();

  while (*data) ++data;

  return {ptr.get(), data};
}
