/* Tags the HUD ammo icon draw with its ammo slot so the renderer can swap in
 * embedded HD art (port/fast3d/hd_ammo_icons, HD HUD Icons setting). The
 * game's 32-bit ammo icons do not reproduce the HD pack's texture CRCs, so
 * the slot, not the pixels, identifies each icon. */
#include <ultra64.h>
#include <bondtypes.h>
#include "port_hud.h"

extern struct sImageTableEntry *ammo9mmimage, *rifleammoimage, *shotgunammoimage, *knifeammoimage,
    *glaunchammoimage, *rocketammoimage, *genericmineammoimage, *grenadeammoimage, *magnumammoimage,
    *goldengunammoimage, *remotemineammoimage, *timedmineammoimage, *proxmineammoimage, *tankammoimage;

/* gunfire.c portAmmoIconPtr order; the embedded art uses the same slots. */
s32 portHudAmmoIconSlot(const struct sImageTableEntry *image)
{
    struct sImageTableEntry *const slots[] = {
        ammo9mmimage, rifleammoimage, shotgunammoimage, knifeammoimage, glaunchammoimage,
        rocketammoimage, genericmineammoimage, grenadeammoimage, magnumammoimage,
        goldengunammoimage, remotemineammoimage, timedmineammoimage, proxmineammoimage, tankammoimage,
    };
    s32 i;
    if (!image) return -1;
    for (i = 0; i < (s32)(sizeof(slots) / sizeof(slots[0])); i++) if (slots[i] == image) return i;
    return -1;
}

Gfx *portHudAmmoIconTag(Gfx *gdl, const struct sImageTableEntry *image)
{
    gDPNoOpTag(gdl++, 0x5B000000u | (u32)(portHudAmmoIconSlot(image) + 1));
    return gdl;
}
