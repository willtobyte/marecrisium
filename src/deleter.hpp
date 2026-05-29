#pragma once

struct SDL_Deleter final {
  template <typename T>
    requires requires(T* p) { SDL_CloseGamepad(p); } ||
             requires(T* p) { SDL_DestroyTexture(p); } ||
             requires(T* p) { SDL_free(p); }
  void operator()(T* ptr) const {
    if (!ptr) [[unlikely]] return;

    if constexpr (requires { SDL_CloseGamepad(ptr); }) SDL_CloseGamepad(ptr);
    else if constexpr (requires { SDL_DestroyTexture(ptr); }) SDL_DestroyTexture(ptr);
    else if constexpr (requires { SDL_free(ptr); }) SDL_free(ptr);
  }
};

struct SPNG_Deleter final {
  void operator()(spng_ctx* context) const {
    if (!context) [[unlikely]] return;

    spng_ctx_free(context);
  }
};

struct OggOpusFile_Deleter final {
  void operator()(OggOpusFile* ptr) const {
    if (!ptr) [[unlikely]] return;

    op_free(ptr);
  }
};

struct PHYSFS_Deleter final {
  template <typename T>
  void operator()(T* ptr) const {
    if (!ptr) [[unlikely]] return;

    if constexpr (std::is_same_v<T, PHYSFS_File>) {
      PHYSFS_close(ptr);
    } else if constexpr (std::is_same_v<T, char*>) {
      PHYSFS_freeList(ptr);
    }
  }
};
