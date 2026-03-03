#include "io.hpp"

bool io::exists(std::string_view filename) noexcept {
  return PHYSFS_exists(filename.data());
}

std::vector<uint8_t> io::read(std::string_view filename) {
  const auto ptr = std::unique_ptr<PHYSFS_File, PHYSFS_Deleter>(PHYSFS_openRead(filename.data()));
  if (!ptr) [[unlikely]]
    throw std::runtime_error(std::format("[PHYSFS_openRead] error while opening file: {}", filename));

  const auto length = PHYSFS_fileLength(ptr.get());
  assert(length >= 0 && "[PHYSFS_fileLength] failed to get file length");

  const auto amount = static_cast<std::size_t>(length);
  std::vector<uint8_t> buffer(amount);
  const auto result = PHYSFS_readBytes(ptr.get(), buffer.data(), amount);
  assert(result == length && "[PHYSFS_readBytes] failed to read expected number of bytes");

  return buffer;
}

std::vector<std::string> io::enumerate(std::string_view directory) {
  std::unique_ptr<char*[], PHYSFS_Deleter> ptr(PHYSFS_enumerateFiles(directory.data()));
  if (!ptr) [[unlikely]]
    throw std::runtime_error(std::format("[PHYSFS_enumerateFiles] error while enumerating directory: {}", directory).c_str());

  auto* const *data = ptr.get();

  auto n = 0uz;
  while (data[n] != nullptr) ++n;

  std::vector<std::string> entries;
  entries.reserve(n);

  for (auto i = 0uz; i < n; ++i) {
    entries.emplace_back(data[i]);
  }

  return entries;
}
