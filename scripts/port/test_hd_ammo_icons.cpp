#include <cassert>
#include <vector>
#include "hd_ammo_icons.cpp"

int main()
{
    using namespace hd_ammo_icons;
    std::vector<uint8_t> rgba;
    unsigned width = 0, height = 0, images = 0;
    for (int slot = 0; slot < SLOTS; ++slot) {
        const HdAmmoIcon& icon = hdAmmoIcons[slot];
        if (!available(slot)) {
            assert(slot == 6 && !load(slot, 16, 16, rgba, width, height)); /* plain mine keeps its image */
            continue;
        }
        /* The native TMEM tile pads rows to 8 bytes (two 32-bit texels). */
        const unsigned tileWidth = (icon.native_width+1u) & ~1u, tileHeight = icon.native_height;
        assert(load(slot, tileWidth, tileHeight, rgba, width, height));
        assert(width == tileWidth*HD_AMMO_ICON_SCALE && height == tileHeight*HD_AMMO_ICON_SCALE);
        bool art = false, clearPadding = true;
        for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
            const uint8_t alpha = rgba[(size_t(y)*width+x)*4+3];
            if (x >= icon.width || y >= icon.height) clearPadding &= alpha == 0;
            else art |= alpha != 0;
        }
        assert(art && clearPadding);
        /* A wider tile keeps the art at the top-left, where native UVs point. */
        assert(load(slot, tileWidth+2, tileHeight, rgba, width, height) && width == (tileWidth+2)*HD_AMMO_ICON_SCALE);
        /* A tile smaller than the visible icon cannot hold it. */
        assert(!load(slot, icon.native_width-1, tileHeight, rgba, width, height));
        assert(!load(slot, tileWidth, icon.native_height-1, rgba, width, height));
        ++images;
    }
    assert(images == 13 && !available(-1) && !available(SLOTS));
    return 0;
}
