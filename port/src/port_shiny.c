/* FORK-058: the display-list tag that brackets a shiny weapon's first-person
 * draw (gunfire.c). gfx_pc.cpp reads it as G_NOOP 0x5E with the item in the
 * low byte, 0 = closed; port/fast3d/gfx_shiny.cpp decides whether that gun
 * takes the Shiny Gold Guns look. */
#include <ultra64.h>
#include <bondconstants.h>
#include "port_shiny.h"

static s32 portShinyItem(s32 item)
{
    return item == ITEM_RUGER || item == ITEM_GOLDENGUN || item == ITEM_SILVERWPPK || item == ITEM_GOLDWPPK;
}

Gfx *portShinyWeaponTag(Gfx *gdl, s32 item, s32 on)
{
    if (!portShinyItem(item)) return gdl;
    gDPNoOpTag(gdl++, 0x5E000000u | (on ? (u32)item & 0xFFu : 0u));
    return gdl;
}
