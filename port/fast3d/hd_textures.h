#ifndef MIDAS_HD_TEXTURES_H
#define MIDAS_HD_TEXTURES_H
/* Player-installed HD texture packs (FORK-053): the renderer's side. */
#include "hd_texture_pack.h"
namespace hd_textures {
/* Render thread, once a frame: true when the usable pack changed, so cached
 * textures must be dropped. */
bool beginFrame();
/* The replacement for a native tile. The download's images (a few hundred
 * pixels a side) are read at once: now, in image. A custom pack's may be the
 * artist's multi-megapixel masters, so they are read on worker threads and
 * reduced to at most kMaxScale times native (the HD tier; there is no 4K):
 * later, under request, while the original draws. */
enum class Found { none, now, later };
constexpr unsigned kMaxScale = 8;
/* maskWrapped: the tile draws at its wrap mask's size (the renderer rebuilds
 * it there), and only images made for that (Remap::masked) apply. */
Found lookup(const Tile &tile, Image &image, uint32_t &request, bool maskWrapped = false);
/* Render thread: one finished custom image, checked and reduced; false when
 * none is waiting. An unusable image never arrives (the original stays), and
 * nothing requested before the pack changed arrives after it. */
bool finished(uint32_t &request, Image &image);
/* GE_HD_TRACE: report, once, each texture the first-person weapon or hands
 * draw (they stay original), naming the pack file when one has it. */
void kept(const Tile &tile);

/* HUD ammo icons (gunfire.c portAmmoIconPtr order): the pack names them by
 * Rice keys the game's own 32-bit icons never reproduce, so the HUD tags
 * each draw with its slot instead (port_hud_ammo.c). */
constexpr int kAmmoSlots = 14;
constexpr unsigned kAmmoScale = 8;
/* Whether the installed pack has art for the slot (the plain mine has none). */
bool ammo_available(int slot);
/* The slot's art resized to its icon's visible size times kAmmoScale, laid
 * over a canvas the size of the native tile (padded width x rows) times
 * kAmmoScale, so native UVs keep their meaning. */
bool ammo_icon(int slot, unsigned tile_width, unsigned tile_height, Image &image);
}
#endif
