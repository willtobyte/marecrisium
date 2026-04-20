#include "filesystem.hpp"

void filesystem::mount(std::string_view filename, std::string_view mountpoint) {
  if (!PHYSFS_mount(filename.data(), mountpoint.data(), true)) [[unlikely]] {
    throw std::runtime_error{std::format("[PHYSFS_mount] failed to mount {} to {}. reason: {}", filename, mountpoint, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()))};
  }
}

void filesystem::try_mount(std::string_view filename, std::string_view mountpoint) {
  PHYSFS_mount(filename.data(), mountpoint.data(), false);
}
