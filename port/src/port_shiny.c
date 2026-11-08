/* FORK-058: the display-list tags that bracket a shiny weapon's first-person
 * draw (gunfire.c). gfx_pc.cpp reads G_NOOP 0x5E with the item in the low
 * byte (0 = closed); port/fast3d/gfx_shiny.cpp decides whether that gun takes
 * the Shiny Gold Guns look. Before opening, the camera position goes out as
 * three 0x5F tags (axis in bits 22-23, a signed whole-unit value in bits
 * 0-21) and the level's sky colour as 0x60 (RGB in the low 24 bits), so the
 * reflection can follow the player through the level. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/bgfog.h"
#include "game/player.h"
#include "port_shiny.h"

static s32 portShinyItem(s32 item)
{
    return item == ITEM_RUGER || item == ITEM_GOLDENGUN || item == ITEM_SILVERWPPK || item == ITEM_GOLDWPPK;
}

static Gfx *portShinyPositionTag(Gfx *gdl, u32 axis, f32 value)
{
    const s32 limit = (1 << 21) - 1;
    s32 v = (s32)value;
    if (v > limit) v = limit;
    if (v < -limit) v = -limit;
    gDPNoOpTag(gdl++, 0x5F000000u | (axis << 22) | ((u32)v & 0x3FFFFFu));
    return gdl;
}

Gfx *portShinyWeaponTag(Gfx *gdl, s32 item, s32 on)
{
    if (!portShinyItem(item)) return gdl;
    if (on && g_CurrentPlayer != NULL) {
        const CurrentEnvironmentRecord *env = fogGetCurrentEnvironmentp();
        gdl = portShinyPositionTag(gdl, 0, g_CurrentPlayer->pos.x);
        gdl = portShinyPositionTag(gdl, 1, g_CurrentPlayer->pos.y);
        gdl = portShinyPositionTag(gdl, 2, g_CurrentPlayer->pos.z);
        gDPNoOpTag(gdl++, 0x60000000u | ((u32)env->Red << 16) | ((u32)env->Green << 8) | (u32)env->Blue);
    }
    gDPNoOpTag(gdl++, 0x5E000000u | (on ? (u32)item & 0xFFu : 0u));
    return gdl;
}
