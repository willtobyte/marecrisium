#pragma once

#include "font.hpp"
#include "pixmap.hpp"
#include "sound.hpp"

#include "fontpool.hpp"
#include "particlepool.hpp"
#include "pixmappool.hpp"
#include "soundpool.hpp"
#include "sourcepool.hpp"

struct resources {
  fontpool font;
  particlepool particle;
  pixmappool pixmap;
  soundpool sound;
  sourcepool source;
};
