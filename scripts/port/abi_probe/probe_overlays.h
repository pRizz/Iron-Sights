/* Layout checks behind the overlay review's verdicts (LP64-005 Task 3; the
 * sites are in scripts/port/lp64_overlays_reviewed.txt, each `same` or
 * `overlay-64` line naming its assertion here by its message). Included by
 * probe.c after the game headers, so it is built at both pointer widths by
 * scripts/port/test_abi_probe.sh. Each assertion holds at both widths and
 * fails if either layout drifts. */
#include "game/chr.h"
#include "game/image.h"
#include "game/explosion.h"
#include "port_romconv_stage.h"

/* Scalar records read as arrays of their scalars. */
_Static_assert(sizeof(coord3d) == 3 * sizeof(f32) && offsetof(coord3d, f) == 0 && offsetof(coord3d, z) == 2 * sizeof(f32),
               "coord3d is three floats");
_Static_assert(sizeof(Mtxf) == 16 * sizeof(f32) && offsetof(Mtxf, m) == 0 && sizeof(RenderPosView) == sizeof(Mtxf),
               "Mtxf and RenderPosView are 16 floats");
_Static_assert(offsetof(Vtx, v.ob) == 0 && sizeof(((Vtx *) 0)->v.ob) == 3 * sizeof(s16),
               "a vertex starts with its three s16 coordinates");
_Static_assert(sizeof(ExplosionDetailsRecord) == 7 * sizeof(s16) && offsetof(ExplosionDetailsRecord, Seed) == sizeof(s16),
               "an explosion details record is a type and six seeds");
_Static_assert(sizeof(struct image_entry) == 8, "an image entry is 8 bytes");
/* bg.c's Unk80081600 (declared in bg.c) starts with a bbox2d written by name. */
_Static_assert(sizeof(bbox2d) == 4 * sizeof(f32), "bbox2d is four floats");
_Static_assert(offsetof(ObjectRecord, type) == 3 && sizeof(((ObjectRecord *) 0)->type) == 1,
               "an object's type is its fourth byte");

/* Collision: the 64-bit stan converter keeps the tiles' layout (R10), and
 * the record load_bg_file prints is two pointers. */
_Static_assert(offsetof(StandTile, points) == 8 && sizeof(StandTilePoint) == 8, "a stan tile's points follow 8 header bytes");
_Static_assert(offsetof(struct StanPrefixRecord, ptr_firstroom) == sizeof(void *),
               "the stan prefix record's first room is its second pointer");

/* Model node records the game reads as three floats at the record's start:
 * the node a model's switch slot names for a position (gunfire.c's flash
 * nodes) is one of these, and the converter writes their floats at 0. The
 * group, simple group and interlink are pinned in probe.c. */
_Static_assert(offsetof(ModelRoData_Op11Record, pos) == 0 && offsetof(ModelRoData_GunfireRecord, Offset) == 0
                   && offsetof(ModelRoData_BSPRecord, Point) == 0,
               "the other node records that start with their position keep it at 0");
/* romconv_model_records.c writes Op11's words 0 to 2 at pos and widens 3 to
 * 15 into unk0c[3..15]: the widened words start after the position. */
_Static_assert(offsetof(ModelRoData_Op11Record, unk0c[3]) >= sizeof(coord3d), "Op11's widened words follow its position");

/* The character record's action union, read through act_init.padding[] by
 * int index where the active act is all scalars before the member. */
#define ACT_INT_AT(act, member, index)                                                                    \
    _Static_assert(offsetof(struct act, member) == (index) * sizeof(int)                                \
                       && sizeof(((struct act *) 0)->member) == sizeof(int),                             \
                   #act "." #member " is act_init.padding[" #index "]")
ACT_INT_AT(act_anim, holdLastFrame, 1);
ACT_INT_AT(act_anim, idleOnEnd, 3);
ACT_INT_AT(act_dead, allowfade, 0);
ACT_INT_AT(act_argh, notifychrindex, 0);
ACT_INT_AT(act_die, notifychrindex, 0);
/* chrlvTickAttackCommon reads act_attack's members for ACT_ATTACKROLL too. */
_Static_assert(offsetof(struct act_attack, unk38) == offsetof(struct act_attackroll, unk38)
                   && offsetof(struct act_attack, unk3a) == offsetof(struct act_attackroll, unk3a),
               "act_attack's aim arrays are act_attackroll's");

/* Runs: a member followed by filler members. Each room, from the head to the
 * next named member (or the record's end), holds the most any code writes
 * through the head's address; the writers are named in the reviewed list. */
#define RUN_HOLDS(type, head, next, bytes) \
    _Static_assert(offsetof(type, next) - offsetof(type, head) >= (bytes), #type "." #head " run holds " #bytes)
#define RUN_HOLDS_END(type, head, bytes) \
    _Static_assert(sizeof(type) - offsetof(type, head) >= (bytes), #type "." #head " run holds " #bytes)
RUN_HOLDS(ModelRwData_HeaderRecord, pos, unk24, sizeof(coord3d));
RUN_HOLDS(ModelRwData_HeaderRecord, unk24, unk34, sizeof(coord3d));
RUN_HOLDS_END(ModelRwData_HeaderRecord, unk4c, sizeof(coord3d));
RUN_HOLDS(struct modeldata_root, unk24, unk34, sizeof(coord3d));
RUN_HOLDS_END(struct modeldata_root, unk4c, sizeof(coord3d));
RUN_HOLDS(ModelRoData_Op11Record, pos, BoundingVolumeRadius, sizeof(coord3d));
RUN_HOLDS_END(struct act_preargh, pos, sizeof(coord3d));
RUN_HOLDS(struct act_gopos, waydata, unk9c, sizeof(struct waydata));
RUN_HOLDS_END(BeamRecord, delta, sizeof(coord3d));
RUN_HOLDS(Projectile, unk10, unk1C, sizeof(coord3d));
RUN_HOLDS(Projectile, mtx, ownerprop, sizeof(Mtxf));
RUN_HOLDS(CCTVRecord, unk84, timer, sizeof(Mtxf));
/* sub_GAME_7F03ECC0 (chrprop.c) writes up to 8 hull points through the
 * tank's rect4f, which has 4: the fillers after it are the other 4. */
RUN_HOLDS(TankRecord, rect, is_firing_tank, 8 * sizeof(struct coord2d));
RUN_HOLDS(SetupIntroCamera, unk14, lang1c, sizeof(fs15_16));
RUN_HOLDS(struct hand, field_8EC, sway_pos_x, sizeof(Mtxf));
RUN_HOLDS(struct hand, field_B58, field_B68, sizeof(coord3d));
RUN_HOLDS(struct player, current_room_pos, field_5C, sizeof(coord3d));
RUN_HOLDS(struct player, collision_bounds, field_D0, sizeof(struct rect4f));
RUN_HOLDS(struct player, field_3B8, field_3C4, sizeof(coord3d));
RUN_HOLDS(struct player, field_2A18, field_2A30, 2 * sizeof(coord3d));
RUN_HOLDS(struct FlyingParticles, rotation_drift, unk34, sizeof(coord3d));
RUN_HOLDS(s_shattered_window_piece, angvel, field_0x34, sizeof(coord3d));
RUN_HOLDS(s_moving_bullet_spark, velocity, unk38, sizeof(coord3d));

/* chrprop.c tests an image entry's hitSound as the low nibble of its first
 * byte: GCC allocates the first bit-field there at both widths. */
static int checkImageEntryHitSound(void)
{
    struct image_entry entry;

    memset(&entry, 0, sizeof(entry));
    entry.hitSound = 5;
    entry.hitTexture = 9;
    if ((((u8 *) &entry)[0] & 0xf) != 5)
    {
        printf("ABI probe FAIL: an image entry's hitSound is not the low nibble of its first byte\n");
        return 1;
    }
    return 0;
}
