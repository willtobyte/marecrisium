#pragma once

template <auto Free>
struct Deleter final {
  template <typename T>
  void operator()(T *ptr) const noexcept {
    Free(ptr);
  }
};

struct SDL_Deleter final {
  template <typename T>
  void operator()(T *ptr) const noexcept {
    SDL_free(ptr);
  }

  void operator()(SDL_Gamepad *ptr) const noexcept { SDL_CloseGamepad(ptr); }
  void operator()(SDL_Texture *ptr) const noexcept { SDL_DestroyTexture(ptr); }
};

using STBI_Deleter = Deleter<stbi_image_free>;
using STB_Vorbis_Deleter = Deleter<stb_vorbis_close>;
using SQLite_Database_Deleter = Deleter<sqlite3_close>;
using SQLite_Statement_Deleter = Deleter<sqlite3_finalize>;
using YYJSON_Doc_Deleter = Deleter<yyjson_doc_free>;
using YYJSON_Mut_Doc_Deleter = Deleter<yyjson_mut_doc_free>;
using STD_Deleter = Deleter<std::free>;
using ZSTD_DCtx_Deleter = Deleter<ZSTD_freeDCtx>;
using ZSTD_DDict_Deleter = Deleter<ZSTD_freeDDict>;
