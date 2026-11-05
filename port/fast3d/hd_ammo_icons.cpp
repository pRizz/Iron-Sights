#include "hd_ammo_icons.h"
#include <cstring>
#include "port_icon_decode.h"

namespace hd_ammo_icons {
struct HdAmmoIcon {
    uint16_t native_width, native_height; /* the image table's visible size */
    uint16_t width, height;               /* embedded art; 0 = keep the original */
    uint32_t offset, size;
};
#include "hud_icons/hd_ammo_icons.inc"
static_assert(sizeof(hdAmmoIcons)/sizeof(hdAmmoIcons[0]) == SLOTS, "one entry per HUD ammo slot");

bool available(int slot)
{
    return slot >= 0 && slot < SLOTS && hdAmmoIcons[slot].size != 0;
}

bool load(int slot, unsigned tileWidth, unsigned tileHeight, std::vector<uint8_t>& rgba,
          unsigned& width, unsigned& height)
{
    if (!available(slot)) return false;
    const HdAmmoIcon& icon = hdAmmoIcons[slot];
    if (icon.width != icon.native_width*HD_AMMO_ICON_SCALE || icon.height != icon.native_height*HD_AMMO_ICON_SCALE
        || tileWidth < icon.native_width || tileHeight < icon.native_height || tileWidth > 64 || tileHeight > 64
        || icon.offset > sizeof(hdAmmoPixels) || icon.size > sizeof(hdAmmoPixels)-icon.offset) return false;
    static std::vector<uint8_t> art;
    art.resize(size_t(icon.width)*icon.height*4);
    if (!portIconDecode(hdAmmoPixels+icon.offset, icon.size, art.data(), unsigned(art.size()))) return false;
    width = tileWidth*HD_AMMO_ICON_SCALE;
    height = tileHeight*HD_AMMO_ICON_SCALE;
    rgba.assign(size_t(width)*height*4, 0);
    for (unsigned y = 0; y < icon.height; ++y)
        std::memcpy(rgba.data()+size_t(y)*width*4, art.data()+size_t(y)*icon.width*4, size_t(icon.width)*4);
    return true;
}
}
