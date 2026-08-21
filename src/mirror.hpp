#pragma once

namespace mirror {
  enum class value : uint8_t {
    none = SDL_FLIP_NONE,
    horizontal = SDL_FLIP_HORIZONTAL,
    vertical = SDL_FLIP_VERTICAL,
    both = SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL,
  };

  void wire();
}
