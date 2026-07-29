#pragma once

struct SDL_Deleter final {
  template <typename T>
    requires requires(T* p) { SDL_CloseGamepad(p); } ||
             requires(T* p) { SDL_DestroyTexture(p); } ||
             requires(T* p) { SDL_free(p); }
  void operator()(T* ptr) const noexcept {
    if (!ptr) [[unlikely]] return;

    if constexpr (requires { SDL_CloseGamepad(ptr); }) SDL_CloseGamepad(ptr);
    else if constexpr (requires { SDL_DestroyTexture(ptr); }) SDL_DestroyTexture(ptr);
    else if constexpr (requires { SDL_free(ptr); }) SDL_free(ptr);
  }
};

struct STBI_Deleter final {
  void operator()(stbi_uc* ptr) const noexcept {
    stbi_image_free(ptr);
  }
};

struct OggOpusFile_Deleter final {
  void operator()(OggOpusFile* ptr) const noexcept {
    if (!ptr) [[unlikely]] return;

    op_free(ptr);
  }
};
