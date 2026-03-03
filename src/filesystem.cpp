#include "filesystem.hpp"

void filesystem::mount(std::string_view filename, std::string_view mountpoint) {
  const auto result = PHYSFS_mount(filename.data(), mountpoint.data(), true);

  if (!result) [[unlikely]] {
    const auto* const message = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());

    throw std::runtime_error(
      std::format("[PHYSFS_mount] failed to mount {} to {}. reason: {}",
        filename,
        mountpoint,
        message));
  }
}
