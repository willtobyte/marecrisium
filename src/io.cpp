blob::blob(std::unique_ptr<backing, releaser> storage) noexcept
  : storage{std::move(storage)} {}

const uint8_t *blob::data() const noexcept {
  return storage->data();
}

std::size_t blob::size() const noexcept {
  return storage->length;
}

blob::operator std::span<const uint8_t>() const noexcept {
  return {data(), size()};
}

bool io::exists(std::string_view filename) {
  return PHYSFS_exists(filename.data());
}

blob io::read(std::string_view filename) {
  capture capture;
  const auto file = std::unique_ptr<PHYSFS_File, PHYSFS_Deleter>{PHYSFS_openRead(filename.data())};
  if (!file) [[unlikely]]
    throw std::runtime_error{std::format("[PHYSFS_openRead] error while opening file: {}", filename)};

  return capture.finish();
}

std::vector<std::string> io::enumerate(std::string_view directory) {
  std::unique_ptr<char*[], PHYSFS_Deleter> ptr{PHYSFS_enumerateFiles(directory.data())};
  assert(ptr && "[PHYSFS_enumerateFiles] failed to enumerate directory");

  auto **data = ptr.get();

  while (*data) ++data;

  return {ptr.get(), data};
}
