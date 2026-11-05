#ifndef PORT_HD_AMMO_ICONS_H
#define PORT_HD_AMMO_ICONS_H
#include <cstdint>
#include <vector>

/* intermissionfb's HD ammo icons, embedded by HUD ammo slot (gunfire.c
 * portAmmoIconPtr order) in every target (FORK_CHANGES). */
namespace hd_ammo_icons {
constexpr int SLOTS = 14;
bool available(int slot);
/* Lay the slot's art over a canvas the size of the native tile (padded TMEM
 * width x rows) times the art's scale, so native UVs keep their meaning. */
bool load(int slot, unsigned tileWidth, unsigned tileHeight, std::vector<uint8_t>& rgba,
          unsigned& width, unsigned& height);
}
#endif
