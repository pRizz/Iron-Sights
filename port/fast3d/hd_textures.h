#ifndef MIDAS_HD_TEXTURES_H
#define MIDAS_HD_TEXTURES_H
#include "hd_texture_format.h"
namespace hd_textures {
/* Render-thread only. A changed pack generation invalidates GPU/retained caches. */
bool beginFrame();
struct Image { unsigned width, height; std::vector<uint8_t> rgba; };
bool lookup(const Tile& tile, Image& image);
}
#endif
