#pragma once

struct depot final {
  template<typename T, typename... Args>
  decltype(auto) get(Args&&... args) {
    if constexpr (std::same_as<T, ::font>)
      return font.get(std::forward<Args>(args)...);
    else if constexpr (std::same_as<T, ::config>)
      return particle.get(std::forward<Args>(args)...);
    else if constexpr (std::same_as<T, ::pixmap>)
      return pixmap.get(std::forward<Args>(args)...);
    else if constexpr (std::same_as<T, clip>)
      return sound.get(std::forward<Args>(args)...);
    else if constexpr (std::same_as<T, ::spritesheet>)
      return spritesheet.get(std::forward<Args>(args)...);
    else
      static_assert(std::same_as<T, void>, "resource type is not supported");
  }

  fontpool font;
  particlepool particle;
  pixmappool pixmap;
  soundpool sound;
  spritesheetpool spritesheet;
};
