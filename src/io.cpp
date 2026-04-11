#include "io.hpp"

bool io::exists(std::string_view filename) noexcept {
  return PHYSFS_exists(filename.data());
}

std::vector<uint8_t> io::read(std::string_view filename) {
  const auto ptr = std::unique_ptr<PHYSFS_File, PHYSFS_Deleter>{PHYSFS_openRead(filename.data())};
  if (!ptr) [[unlikely]]
    throw std::runtime_error{std::format("[PHYSFS_openRead] error while opening file: {}", filename)};

  const auto bytes = PHYSFS_fileLength(ptr.get());
  if (bytes < 0) [[unlikely]]
    throw std::runtime_error{std::format("[PHYSFS_fileLength] unknown length for: {}", filename)};
  const auto length = static_cast<std::size_t>(bytes);
  std::vector<uint8_t> buffer(length);
  [[maybe_unused]] const auto result = PHYSFS_readBytes(ptr.get(), buffer.data(), length);
  assert(result == static_cast<PHYSFS_sint64>(length) && "[PHYSFS_readBytes] failed to read expected number of bytes");

  return buffer;
}

std::vector<std::string> io::enumerate(std::string_view directory) {
  std::unique_ptr<char*[], PHYSFS_Deleter> ptr{PHYSFS_enumerateFiles(directory.data())};
  if (!ptr) [[unlikely]]
    throw std::runtime_error{std::format("[PHYSFS_enumerateFiles] error while enumerating directory: {}", directory)};

  auto **data = ptr.get();

  while (*data) ++data;

  return {ptr.get(), data};
}
