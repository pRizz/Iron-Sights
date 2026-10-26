/* Throwing-knife presentation only. Native throw/ammo/projectile events stay owned by gunfire.c. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "game/dyn.h"
#include "game/gun.h"
#include "game/model.h"
#include "game/matrixmath.h"
#include "game/player.h"
#include "port_knife_ads.h"

extern void *memset(void *dst, int value, size_t size);
extern void *memcpy(void *dst, const void *src, size_t size);

typedef struct {
    f32 aim, turn, style;
    coord3d offset;
    s32 throwing;
} KnifeAdsState;

static KnifeAdsState knifeStates[4];
static s32 knifeStyle;

s32 portKnifeAdsOptionGet(const char *key, s32 *value)
{
    if (strcmp(key, "knife_ads") != 0) return FALSE;
    *value = knifeStyle;
    return TRUE;
}

s32 portKnifeAdsOptionSet(const char *key, s32 value)
{
    if (strcmp(key, "knife_ads") != 0) return FALSE;
    knifeStyle = value == 1;
    return TRUE;
}

void portKnifeAdsReset(void) { memset(knifeStates, 0, sizeof(knifeStates)); }

static f32 knifeApproach(f32 current, f32 target, f32 step)
{
    /* The inherited math header predates fminf/fmaxf; avoid implicit-call ABI differences. */
    if (current < target) return current + step < target ? current + step : target;
    return current - step > target ? current - step : target;
}

static s32 knifeThrowState(s32 action)
{
    return action >= GUN_ANIM_STATE_THROWKNIFE_DRAW && action <= GUN_ANIM_STATE_THROWKNIFE_RECOVER;
}

void portKnifeAdsTick(s32 player, s32 available, f32 blend, f32 delta)
{
    KnifeAdsState *state;
    s32 action;
    if (player < 0 || player >= 4) return;
    state = &knifeStates[player];
    if (!available || getCurrentPlayerWeaponId(GUNRIGHT) != ITEM_THROWKNIFE) {
        memset(state, 0, sizeof(*state));
        return;
    }
    action = g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state;
    state->throwing = knifeThrowState(action);
    state->style = knifeApproach(state->style, knifeStyle, delta / 9.0f);
    /* A committed throw completes its regrip even if aim is released halfway. */
    state->aim = knifeApproach(state->aim, state->throwing ? 0 : blend, delta / 6.0f);
    state->turn = knifeApproach(state->turn, state->throwing ? 0 : state->style * blend, delta / 6.0f);
}

/* The stock blade grip and the handle's center, in the native knife mesh's units.
 * Rotate the knife around the moving grip point, without deforming its hand. */
static void knifeLocalTransform(f32 turn, Mtxf *matrix)
{
    const coord3d bladeGrip = {0, 50, 65};
    const coord3d handleGrip = {0, 270, 150};
    coord3d grip;
    Mtxf pitch, yaw;
    s32 axis;
    matrix_4x4_set_rotation_around_x(turn * M_PI_F, &pitch);
    /* Show a little blade face instead of an unreadable edge-on silver line. */
    matrix_4x4_set_rotation_around_y(turn * 0.5f, &yaw);
    matrix_4x4_multiply_homogeneous(&yaw, &pitch, matrix);
    for (axis = 0; axis < 3; axis++) grip.f[axis] = bladeGrip.f[axis] + (handleGrip.f[axis] - bladeGrip.f[axis]) * turn;
    for (axis = 0; axis < 3; axis++)
        matrix->m[3][axis] = bladeGrip.f[axis] - grip.x * matrix->m[0][axis]
            - grip.y * matrix->m[1][axis] - grip.z * matrix->m[2][axis];
}

static void knifePoint(const Mtxf *matrix, const coord3d *point, coord3d *out)
{
    s32 axis;
    for (axis = 0; axis < 3; axis++) out->f[axis] = matrix->m[3][axis]
        + point->x * matrix->m[0][axis] + point->y * matrix->m[1][axis] + point->z * matrix->m[2][axis];
}

void portKnifeAdsPose(s32 hand, s32 item, const Mtxf *basis, coord3d *origin)
{
    const s32 player = get_cur_playernum();
    KnifeAdsState *state;
    s32 axis;
    if (hand != GUNRIGHT || item != ITEM_THROWKNIFE || player < 0 || player >= 4) return;
    state = &knifeStates[player];
    if (!state->throwing) {
        const coord3d butt = {1, 338, 150}, tip = {1, -97, 1};
        coord3d sight, turned, camera, target;
        Mtxf local;
        knifeLocalTransform(state->turn, &local);
        for (axis = 0; axis < 3; axis++) sight.f[axis] = butt.f[axis] + (tip.f[axis] - butt.f[axis]) * state->style;
        knifePoint(&local, &sight, &turned);
        knifePoint(basis, &turned, &camera);
        target = (coord3d){-camera.x * 0.1f, -camera.y * 0.1f, -34.0f - 61.0f * state->style};
        if (state->aim > 0.8f) {
            static s32 logged;
            const char *audit = getenv("GE_ADS_AUDIT");
            if (!logged && audit && atoi(audit) == ITEM_THROWKNIFE) {
                extern void portConLogf(const char *fmt, ...);
                portConLogf("ge: knife-pose: basis=(%.2f %.2f %.2f) origin=(%.2f %.2f %.2f) target=(%.2f %.2f %.2f) sight=(%.2f %.2f %.2f) turn=%.2f\n",
                    basis->m[0][0], basis->m[1][1], basis->m[2][2], origin->x, origin->y, origin->z,
                    target.x, target.y, target.z, turned.x, turned.y, turned.z, state->turn);
                logged = TRUE;
            }
        }
        for (axis = 0; axis < 3; axis++) state->offset.f[axis] = target.f[axis] - origin->f[axis];
    }
    for (axis = 0; axis < 3; axis++) origin->f[axis] += state->offset.f[axis] * state->aim;
}

typedef struct { ModelNode *node; union ModelRoData *original; } KnifePatch;

/* Copy only the knife's display list, replacing its root matrix load. Materials,
 * vertices, UVs and the hand's lists remain the ROM's. Unknown models fall back. */
static s32 knifePatchNode(ModelNode *node, Mtxf *matrix, KnifePatch *patches, s32 *count)
{
    s32 i, length, matrixLoad = FALSE;
    Gfx *copy;
    union ModelRoData *data;
    if (*count >= 3 || node->Opcode != MODELNODE_OPCODE_DL) return FALSE;
    if (!node->Data->DisplayList.Primary || node->Data->DisplayList.Secondary) return FALSE;
    for (length = 0; length < 1024; length++) {
        const Gfx *command = &node->Data->DisplayList.Primary[length];
        const u32 op = command->words.w0 >> 24;
        if (op == G_DL || (op == G_MTX && command->words.w1 != 0x03000000)) return FALSE;
        if (op == G_MTX) matrixLoad = TRUE;
        if (op == (u8)G_ENDDL) break;
    }
    if (length == 1024 || !matrixLoad) return FALSE;
    data = dynAllocate(sizeof(*data));
    copy = dynAllocate((length + 1) * sizeof(*copy));
    if (!data || !copy) return FALSE;
    *data = *node->Data;
    memcpy(copy, data->DisplayList.Primary, (length + 1) * sizeof(*copy));
    for (i = 0; i < length; i++) if (copy[i].words.w0 >> 24 == G_MTX) copy[i].words.w1 = osVirtualToPhysical(matrix);
    data->DisplayList.Primary = copy;
    patches[*count] = (KnifePatch){node, node->Data};
    ++*count;
    node->Data = data;
    return TRUE;
}

static s32 knifePatchTree(ModelNode *node, Mtxf *matrix, KnifePatch *patches, s32 *count)
{
    while (node) {
        if (node->Opcode == MODELNODE_OPCODE_DL) {
            if (!knifePatchNode(node, matrix, patches, count)) return FALSE;
        } else if (node->Child && !knifePatchTree(node->Child, matrix, patches, count)) return FALSE;
        node = node->Next;
    }
    return TRUE;
}

void portKnifeAdsDraw(ModelRenderData *renderdata, Model *model, s32 item, s32 hand)
{
    const s32 player = get_cur_playernum();
    KnifePatch patches[3];
    s32 count = 0, i, valid;
    Mtxf local, posed, *fixed;
    if (hand != GUNRIGHT || item != ITEM_THROWKNIFE || player < 0 || player >= 4
        || knifeStates[player].turn <= 0 || model->obj->numSwitches < 16
        || !model->obj->Switches[14] || !model->obj->Switches[15]) {
        subdraw(renderdata, model);
        return;
    }
    fixed = dynAllocate(sizeof(*fixed));
    if (!fixed) { subdraw(renderdata, model); return; }
    knifeLocalTransform(knifeStates[player].turn, &local);
    matrix_4x4_multiply_homogeneous(&model->render_pos[0].pos, &local, &posed);
    matrix_4x4_f32_to_s32(&posed, fixed);
    valid = knifePatchTree(model->obj->Switches[14]->Child, fixed, patches, &count)
        && knifePatchTree(model->obj->Switches[15]->Child, fixed, patches, &count);
    if (!valid) for (i = 0; i < count; i++) patches[i].node->Data = patches[i].original;
    subdraw(renderdata, model);
    for (i = 0; i < count; i++) patches[i].node->Data = patches[i].original;
}
