/* Offline icon capture harness. Enabled only by the generation script. */
#include <stdlib.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/dyn.h"
#include "game/gun.h"
#include "game/matrixmath.h"
#include "game/model.h"
#include "game/objecthandler.h"
#include "game/options.h"
#include "game/player.h"
#include "port_weapon_export.h"
#include "port_wardrobe.h"
#include "port_icon_cache_format.h"

/* Each image takes PORT_ICON_FRAMES frames: PORT_ICON_LEAD frames for the
 * model to settle (3 sufficed when measured; 2 failed the repeat check), then
 * black, white, black and white captures on consecutive frames. */
#define PORT_ICON_LEAD 4
#define PORT_ICON_FRAMES (PORT_ICON_LEAD + 4)
#define PORT_TANK_ICON_YAW -35.0f
#define PORT_TANK_ICON_TARGET_Y 0.08f
/* The rifle butt turns the sniper rifle stock-first, muzzle angled down so the
 * butt is raised to strike. */
#define PORT_BUTT_ICON_YAW 180.0f
#define PORT_BUTT_ICON_ROLL -20.0f

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

#ifdef GE_VIDEO
/* The tank has no first-person gun model, so its strip image is the world
 * prop, framed by its bounding sphere from the same near-side camera angle. */
static Gfx *portWeaponExportTank(Gfx *gdl)
{
    extern struct ItemModelFileRecord PitemZ_entries[];
    extern void matrix_4x4_7F058C64(void);
    extern void matrix_4x4_7F058C88(void);
    ItemModelFileRecord *record = &PitemZ_entries[PROP_TANK];
    ModelFileHeader *header = record->header;
    ModelRenderData renderdata = {0};
    Model model = {0};
    Mtxf rotation, view, transform, *matrices;
    f32 radius, distance;
    s32 i, oldDistanceDisabled;
    if (header->RootNode == NULL) fileLoad(header, record->filename);
    modelCalculateRwDataLen(header);
    modelInit(&model, header, dynAllocate(header->numRecords * 4 + 16));
    /* Match the parked tank's parts (propobj.c): no cannon muzzle flash. */
    if (header->numSwitches > 8 && header->Switches[7] != NULL)
        modelGetNodeRwData(&model, header->Switches[7])->Gunfire.visible = 0;
    if (header->numSwitches > 8 && header->Switches[8] != NULL)
        modelGetNodeRwData(&model, header->Switches[8])->Switch.visible = 0;
    radius = header->BoundingVolumeRadius * record->scale;
    distance = radius * 2.3f;
    matrix_4x4_set_rotation_around_y(PORT_TANK_ICON_YAW * 0.01745329252f, &rotation);
    for (i = 0; i < 3; i++) {
        rotation.m[i][0] *= record->scale;
        rotation.m[i][1] *= record->scale;
        rotation.m[i][2] *= record->scale;
    }
    matrix_4x4_set_lookat_target(&view, distance*0.82f, radius*PORT_TANK_ICON_TARGET_Y + distance*0.32f, distance*0.47f,
        0, radius*PORT_TANK_ICON_TARGET_Y, 0, 0, 1, 0);
    matrix_4x4_multiply(&view, &rotation, &transform);
    matrices = dynAllocate(header->numMatrices * sizeof(Mtxf));
    renderdata.basemtx = &transform;
    renderdata.mtxlist = matrices;
    renderdata.zbufferenabled = TRUE;
    renderdata.flags = 3;
    renderdata.PropType = 9;
    oldDistanceDisabled = g_ModelDistanceDisabled;
    g_ModelDistanceDisabled = TRUE; /* highest-detail LOD */
    instcalcmatrices(&renderdata, &model);
    renderdata.gdl = gdl;
    subdraw(&renderdata, &model);
    g_ModelDistanceDisabled = oldDistanceDisabled;
    gdl = renderdata.gdl;
    matrix_4x4_7F058C64();
    for (i = 0; i < header->numMatrices; i++) {
        Mtxf value;
        matrix_4x4_copy(&matrices[i], &value);
        matrix_4x4_f32_to_s32((void *)&value, (void *)&matrices[i]);
    }
    matrix_4x4_7F058C88();
    return gdl;
}
#endif

Gfx *portWeaponExportDraw(Gfx *gdl)
{
#ifdef GE_VIDEO
    static s32 frame;
    const s32 slot = frame / PORT_ICON_FRAMES, phase = frame % PORT_ICON_FRAMES;
    s32 item = slot + ITEM_FIST;
    s32 icon, oldCuff;
    s32 capture = 0, white;
    f32 depth, horizontal, vertical;
    Mtxf rotation, tilt, view, transform;
    struct coord3d target;
    Mtx *projection;
    u16 norm;
    const char *only;
    extern Gfx *sub_GAME_7F0A6EE8(Gfx *gdl);
    extern GunModelFileRecord gitem_structs[];
    if (!portWeaponExportEnabled() || !g_CurrentPlayer) return gdl;
    only = getenv("GE_WEAPON_ICON_ITEM");
    if (only) item = atoi(only);
    if (!only && item >= ITEM_TRIGGER) item++;
    icon = item;
    /* The watch laser renders Bond's watch (set_enviro_fog_for_items_in_solo_watch_menu). */
    if (!only && slot >= 30 && slot < 36) { item = ITEM_FIST; icon = 32+slot-30; }
    if (!only && slot == 36) item = ITEM_TANKSHELLS;
    if (!only && slot == 37) { item = ITEM_SNIPERRIFLE; icon = PORT_ICON_RIFLE_BUTT; }
    if (item == ITEM_TANKSHELLS) icon = PORT_ICON_TANK;
    if (slot >= PORT_ICON_IMAGES || (item > ITEM_TASER && icon != PORT_ICON_TANK)
        || item < ITEM_FIST || (only && slot)) return gdl;
    if (item == ITEM_FIST) g_CurrentPlayer->cur_item_weapon_getname = ITEM_FIST;
    frame++;
    if (phase >= PORT_ICON_LEAD) capture = phase - PORT_ICON_LEAD + 1;
    white = capture == 2 || capture == 4;
    /* Capture names are assigned when this list executes, so queued simulation
     * frames cannot mislabel a weapon's pixels. */
    gdl->words.w0 = (u32)0x4f << 24;
    gdl->words.w1 = (icon << 8) | (white << 4) | capture;
    gdl++;
    gdl = sub_GAME_7F0A6EE8(gdl);
    gSPClearGeometryMode(gdl++, G_FOG | G_CULL_BOTH);
    /* Gameplay weapons restore their own depth state. Icon rendering owns
     * its depth setup rather than inheriting it from the preceding gun. */
    gSPSetGeometryMode(gdl++, G_ZBUFFER);
    gDPSetColorDither(gdl++, G_CD_DISABLE);
    gDPSetAlphaDither(gdl++, G_AD_DISABLE);
    projection = dynAllocateMatrix();
    guPerspective(projection, &norm, 35.0f, 4.0f/3.0f, 1.0f, 20000.0f, 1.0f);
    gSPPerspNormalize(gdl++, norm);
    gSPMatrix(gdl++, osVirtualToPhysical(projection), G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    if (icon == PORT_ICON_TANK) return portWeaponExportTank(gdl);
    depth = gitem_structs[item].equip_watch_z * 1.6f;
    horizontal = gitem_structs[item].equip_watch_x;
    vertical = gitem_structs[item].equip_watch_y;
    /* Fixed near-side view with a small view of the top and front. */
    matrix_4x4_set_rotation_around_y(gitem_structs[item].y_rotation * 0.01745329252f, &rotation);
    matrix_4x4_set_rotation_around_z(-gitem_structs[item].x_rotation * 0.01745329252f, &tilt);
    matrix_4x4_multiply_in_place(&tilt, &rotation);
    target.x = 0; target.y = vertical; target.z = horizontal;
    if (icon == PORT_ICON_RIFLE_BUTT) {
        /* Turn about the framed point: rotate the model and its target alike. */
        Mtxf turn, roll;
        matrix_4x4_set_rotation_around_y(PORT_BUTT_ICON_YAW * 0.01745329252f, &turn);
        matrix_4x4_set_rotation_around_x(PORT_BUTT_ICON_ROLL * 0.01745329252f, &roll);
        matrix_4x4_multiply_in_place(&roll, &turn);
        matrix_4x4_multiply_in_place(&turn, &rotation);
        mtx4RotateVecInPlace(&turn, &target);
    }
    matrix_4x4_set_lookat_target(&view, target.x+depth, target.y+depth*0.16f, target.z+depth*0.18f,
        target.x, target.y, target.z, 0, 1, 0);
    /* matrixmath's arguments use column-major multiplication: view must be
     * the left operand so watch rotations do not rotate the camera offset. */
    matrix_4x4_multiply(&view, &rotation, &transform);
    oldCuff = g_CurrentPlayer->bondtype;
    if (icon >= 32) g_CurrentPlayer->bondtype = portWardrobeCuff(icon-32);
    /* Base unarmed stays blue, independent of the stage used for exporting. */
    else if (item == ITEM_FIST) g_CurrentPlayer->bondtype = portWardrobeCuff(0);
    gdl = set_enviro_fog_for_items_in_solo_watch_menu(gdl, item, &transform, 255, 0x00000000);
    g_CurrentPlayer->bondtype = oldCuff;
    return gdl;
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
