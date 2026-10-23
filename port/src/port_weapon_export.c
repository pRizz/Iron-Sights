/* Offline icon capture harness. Enabled only by the generation script. */
#include <stdlib.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/dyn.h"
#include "game/gun.h"
#include "game/matrixmath.h"
#include "game/options.h"
#include "game/player.h"
#include "port_weapon_export.h"

s32 portWeaponExportEnabled(void)
{
#ifdef GE_VIDEO
    static s32 enabled = -1;
    if (enabled < 0) enabled = getenv("GE_WEAPON_ICON_EXPORT") != NULL;
    return enabled;
#else
    return 0;
#endif
}

Gfx *portWeaponExportDraw(Gfx *gdl)
{
#ifdef GE_VIDEO
    static s32 frame;
    const s32 slot = frame / 32, phase = frame % 32;
    s32 item = slot + ITEM_FIST;
    s32 capture = 0, white;
    f32 depth, horizontal, vertical;
    Mtxf rotation, tilt, view, transform;
    Mtx *projection;
    u16 norm;
    const char *only;
    extern Gfx *sub_GAME_7F0A6EE8(Gfx *gdl);
    extern GunModelFileRecord gitem_structs[];
    if (!portWeaponExportEnabled() || !g_CurrentPlayer) return gdl;
    only = getenv("GE_WEAPON_ICON_ITEM");
    if (only) item = atoi(only);
    if (!only && item >= ITEM_WATCHLASER) item++;
    if (!only && item >= ITEM_TRIGGER) item++;
    if (item > ITEM_TASER || item < ITEM_FIST || (only && slot)) return gdl;
    if (item == ITEM_FIST) g_CurrentPlayer->cur_item_weapon_getname = ITEM_FIST;
    frame++;
    white = phase == 10 || phase == 14;
    if (phase == 8) capture = 1;
    if (phase == 10) capture = 2;
    if (phase == 12) capture = 3;
    if (phase == 14) capture = 4;
    /* Capture names are assigned when this list executes, so queued simulation
     * frames cannot mislabel a weapon's pixels. */
    gdl->words.w0 = (u32)0x4f << 24;
    gdl->words.w1 = (item << 8) | (white << 4) | capture;
    gdl++;
    gdl = sub_GAME_7F0A6EE8(gdl);
    gSPClearGeometryMode(gdl++, G_FOG | G_CULL_BOTH);
    gDPSetColorDither(gdl++, G_CD_DISABLE);
    gDPSetAlphaDither(gdl++, G_AD_DISABLE);
    projection = dynAllocateMatrix();
    guPerspective(projection, &norm, 35.0f, 4.0f/3.0f, 1.0f, 20000.0f, 1.0f);
    gSPPerspNormalize(gdl++, norm);
    gSPMatrix(gdl++, osVirtualToPhysical(projection), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    depth = gitem_structs[item].equip_watch_z * 1.6f;
    horizontal = gitem_structs[item].equip_watch_x;
    vertical = gitem_structs[item].equip_watch_y;
    /* Fixed near-side view with a small view of the top and front. */
    matrix_4x4_set_rotation_around_y(gitem_structs[item].y_rotation * 0.01745329252f, &rotation);
    matrix_4x4_set_rotation_around_z(-gitem_structs[item].x_rotation * 0.01745329252f, &tilt);
    matrix_4x4_multiply_in_place(&tilt, &rotation);
    matrix_4x4_set_lookat_target(&view, depth, vertical+depth*0.16f, horizontal+depth*0.18f,
        0, vertical, horizontal, 0, 1, 0);
    /* matrixmath's arguments use column-major multiplication: view must be
     * the left operand so watch rotations do not rotate the camera offset. */
    matrix_4x4_multiply(&view, &rotation, &transform);
    return set_enviro_fog_for_items_in_solo_watch_menu(gdl, item, &transform, 255, 0x00000000);
#else
    return gdl;
#endif
}

LookAt *portWeaponExportLookAt(void)
{
    static LookAt lookat;
    static s32 initialized;
    if (!initialized) {
        Mtx unused;
        /* Metallic materials must not borrow Bond's breathing/intro camera
         * reflection vectors. This basis belongs to the fixed export view. */
        guLookAtReflect(&unused, &lookat, 1, 0.16f, 0.18f, 0, 0, 0, 0, 1, 0);
        initialized = 1;
    }
    return &lookat;
}
