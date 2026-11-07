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
