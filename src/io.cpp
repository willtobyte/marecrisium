bool io::exists(std::string_view filename) {
  return PHYSFS_exists(filename.data());
}

blob io::read(std::string_view filename) {
  const auto ptr = std::unique_ptr<PHYSFS_File, PHYSFS_Deleter>{PHYSFS_openRead(filename.data())};
  if (!ptr) [[unlikely]]
    throw std::runtime_error{std::format("[PHYSFS_openRead] error while opening file: {}", filename)};

  const auto bytes = PHYSFS_fileLength(ptr.get());
  assert(bytes >= 0 && "[PHYSFS_fileLength] unknown length");
  const auto length = static_cast<std::size_t>(bytes);
  auto buffer = std::make_unique_for_overwrite<uint8_t[]>(length);
  [[maybe_unused]] const auto result = PHYSFS_readBytes(ptr.get(), buffer.get(), length);
  assert(result == static_cast<PHYSFS_sint64>(length) && "[PHYSFS_readBytes] failed to read expected number of bytes");

  return {std::move(buffer), length};
}

std::vector<std::string> io::enumerate(std::string_view directory) {
  std::unique_ptr<char*[], PHYSFS_Deleter> ptr{PHYSFS_enumerateFiles(directory.data())};
  assert(ptr && "[PHYSFS_enumerateFiles] failed to enumerate directory");

  auto **data = ptr.get();

  while (*data) ++data;

  return {ptr.get(), data};
}
