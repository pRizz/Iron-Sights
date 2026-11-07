#ifndef MIDAS_HD_TEXTURES_H
#define MIDAS_HD_TEXTURES_H
/* Player-installed HD texture packs (FORK-053): the renderer's side. */
#include "hd_texture_pack.h"
namespace hd_textures {
/* Render thread, once a frame: true when the usable pack changed, so cached
 * textures must be dropped. */
bool beginFrame();
/* The replacement for a native tile, when the pack has one. */
bool lookup(const Tile &tile, Image &image);
}
#endif
