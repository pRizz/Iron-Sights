/* FORK-058: the display-list tags that bracket a shiny weapon's first-person
 * draw (gunfire.c). gfx_pc.cpp reads G_NOOP 0x5E with the item in the low
 * byte, 0 = closed; port/fast3d/gfx_shiny.cpp decides whether that gun takes
 * the Richer Reflections look. The ordinary firearms are bracketed too, for the glint
 * experiment; G_NOOP 0x61 (1 = on, 0 = off) marks draws that never take a
 * glint: the generated sleeve and the native hand pass (port_native_arms.c,
 * model.c); G_NOOP 0x62 carries the ADS blend for the glint's fade. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/bgfog.h"
#include "port_ads.h"
#include "port_shiny.h"

/* the ordinary firearms, PP7 to sniper rifle: the glint experiment's guns */
static s32 portGlintItem(s32 item)
{
    return item >= ITEM_WPPK && item <= ITEM_SNIPERRIFLE;
}

static s32 portShinyItem(s32 item)
{
    return item == ITEM_RUGER || item == ITEM_GOLDENGUN || item == ITEM_SILVERWPPK || item == ITEM_GOLDWPPK
        || portGlintItem(item);
}

Gfx *portShinyWeaponTag(Gfx *gdl, s32 item, s32 on)
{
    if (!portShinyItem(item)) return gdl;
    if (on && portGlintItem(item)) {
        /* the camera's reflection axes, as gunfire.c loads them for the
         * envmapped guns: the glint's world-fixed light turns with the view.
         * The PP7's own draws are not reflection-mapped, so it changes nothing. */
        gSPLookAt(gdl++, sub_GAME_7F078474());
        /* how far the gun is raised to the sights (G_NOOP 0x62, 0-255): the
         * glint fades out in ADS, where it lay over the sight picture */
        gDPNoOpTag(gdl++, 0x62000000u | (u32)(portAdsBlend() * 255.0f + 0.5f));
    } else if (on) {
        /* prototype: the level's sky colour, for the reflection cube's empty
         * space (G_NOOP 0x60; port/fast3d/gfx_envcube.inl) */
        const CurrentEnvironmentRecord *env = fogGetCurrentEnvironmentp();
        gDPNoOpTag(gdl++, 0x60000000u | ((u32)env->Red << 16) | ((u32)env->Green << 8) | (u32)env->Blue);
    }
    gDPNoOpTag(gdl++, 0x5E000000u | (on ? (u32)item & 0xFFu : 0u));
    return gdl;
}

Gfx *portShinyGlintBlock(Gfx *gdl, s32 on)
{
    gDPNoOpTag(gdl++, 0x61000000u | (on ? 1u : 0u));
    return gdl;
}
