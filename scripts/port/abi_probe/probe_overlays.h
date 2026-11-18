/* Layout checks behind the overlay review's verdicts (LP64-005 Task 3; the
 * sites are in scripts/port/lp64_overlays_reviewed.txt, each `same` or
 * `overlay-64` line naming its assertion here by its message). Included by
 * probe.c after the game headers, so it is built at both pointer widths by
 * scripts/port/test_abi_probe.sh. Each assertion holds at both widths and
 * fails when the layout drifts in the way that would make its sites read the
 * wrong bytes; none states what C already guarantees (a first member at 0, a
 * later member after an earlier one). */
#include "game/chr.h"
#include "game/image.h"
#include "game/explosion.h"
#include "port_romconv_stage.h"

#define IS_TYPE(expr, type) _Generic((expr), type: 1, default: 0)

/* Scalar records read as arrays of their scalars. */
_Static_assert(sizeof(coord3d) == 3 * sizeof(f32) && offsetof(coord3d, y) == sizeof(f32)
                   && offsetof(coord3d, z) == 2 * sizeof(f32),
               "coord3d is three floats");
_Static_assert(sizeof(Mtxf) == 16 * sizeof(f32) && sizeof(RenderPosView) == sizeof(Mtxf),
               "Mtxf and RenderPosView are 16 floats");
_Static_assert(IS_TYPE(((Vtx *) 0)->v.ob[0], s16) && offsetof(Vtx, v.ob[2]) == 2 * sizeof(s16),
               "a vertex starts with its three s16 coordinates");
_Static_assert(sizeof(ExplosionDetailsRecord) == 7 * sizeof(s16) && IS_TYPE(((ExplosionDetailsRecord *) 0)->Seed[0], s16)
                   && offsetof(ExplosionDetailsRecord, Seed) == sizeof(s16),
               "an explosion details record is a type and six seeds");
_Static_assert(sizeof(struct image_entry) == 8, "an image entry is 8 bytes");
_Static_assert(offsetof(ObjectRecord, type) == 3 && sizeof(((ObjectRecord *) 0)->type) == 1,
               "an object's type is its fourth byte");
/* objBreakCCTVGlass reads a bounding box record as a model number and six
 * floats (probe.c pins the floats' start at 4). */
_Static_assert(sizeof(((ModelRoData_BoundingBoxRecord *) 0)->ModelNumber) == 4 && sizeof(struct bbox) == 6 * sizeof(f32),
               "a bounding box record is a model number and six floats");

/* Collision: the 64-bit stan converter keeps the tiles' layout (R10), and
 * the record load_bg_file prints at 64 bits is two pointers. */
_Static_assert(offsetof(StandTile, points) == 8 && sizeof(StandTilePoint) == 8, "a stan tile's points follow 8 header bytes");
_Static_assert(sizeof(((struct StanPrefixRecord *) 0)->stanfile) == sizeof(void *)
                   && offsetof(struct StanPrefixRecord, ptr_firstroom) == sizeof(void *)
                   && sizeof(((struct StanPrefixRecord *) 0)->ptr_firstroom) == sizeof(void *),
               "the stan prefix record's first room is its second pointer");

/* Model node records whose first three words are floats at both widths.
 * gunUpdateAndFire reads a gun's switch slot 2 or 4 node's Data as three
 * floats without testing its opcode, and the gun models' nodes come from the
 * ROM, so every record that begins with a position is pinned: its position
 * has the position type and its floats sit at 0, 4 and 8. */
#define POSITION_FIRST(type, member, postype)                                                        \
    _Static_assert(IS_TYPE(((type *) 0)->member, postype) && offsetof(type, member.y) == sizeof(f32), \
                   #type "." #member " is the floats at 0, 4 and 8")
POSITION_FIRST(ModelRoData_GroupRecord, Origin, coord3d);
POSITION_FIRST(ModelRoData_GroupSimpleRecord, Origin, coord3d);
POSITION_FIRST(ModelRoData_InterlinkageRecord, pos, coord3d);
POSITION_FIRST(ModelRoData_Op11Record, pos, coord3d);
POSITION_FIRST(ModelRoData_GunfireRecord, Offset, coord3d);
POSITION_FIRST(ModelRoData_BSPRecord, Point, coord3d);
POSITION_FIRST(ModelRoData_Op14Record, pos, coord3d);
/* A shadow's 2D position is followed by its 2D size: floats at 0 to 15. */
_Static_assert(IS_TYPE(((ModelRoData_ShadowRecord *) 0)->pos, coord2d) && IS_TYPE(((ModelRoData_ShadowRecord *) 0)->size, coord2d)
                   && offsetof(ModelRoData_ShadowRecord, pos.y) == sizeof(f32)
                   && offsetof(ModelRoData_ShadowRecord, size) == 2 * sizeof(f32),
               "ModelRoData_ShadowRecord.pos is the floats at 0, 4 and 8");

/* The character record's action union, read through act_init.padding[] by
 * int index where the active act is all scalars before the member. */
#define ACT_INT_AT(act, member, index)                                                                    \
    _Static_assert(offsetof(struct act, member) == (index) * sizeof(int)                                \
                       && sizeof(((struct act *) 0)->member) == sizeof(int)                              \
                       && sizeof(((struct act_init *) 0)->padding[0]) == sizeof(int),                    \
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

/* sub_GAME_7F03ECC0 (chrprop.c) writes up to 8 hull points through the
 * tank's rect4f, which has 4, and chrpropTestPointInPolygon reads the rect
 * with the tank's count: the room up to is_firing_tank holds the 8. */
_Static_assert(offsetof(TankRecord, is_firing_tank) - offsetof(TankRecord, rect) >= 8 * sizeof(struct coord2d),
               "TankRecord.rect run holds 8 * sizeof(struct coord2d)");

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
