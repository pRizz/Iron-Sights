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
#include "game/file.h"
#include "game/initanitable.h"
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

/* Batch 2, the offset sites. A float and an integer reinterpreted in place
 * (`*(s32 *) &f`, `*(f32 *) &i`) read the same 4 bytes at both widths. */
_Static_assert(sizeof(f32) == 4 && sizeof(s32) == 4, "f32 and s32 are both 4 bytes");
/* prop.c and propobj.c read these setup members as the s32 the file holds,
 * then store them back as floats: each must stay a 4-byte f32. */
#define SETUP_F32(type, member) IS_TYPE(((type *) 0)->member, f32)
_Static_assert(SETUP_F32(ObjectRecord, damage) && SETUP_F32(CCTVRecord, unkCC) && SETUP_F32(CCTVRecord, unkD0)
                   && SETUP_F32(CCTVRecord, unkDC) && SETUP_F32(CCTVRecord, unkE8) && SETUP_F32(AutogunRecord, speed)
                   && SETUP_F32(AutogunRecord, aimdist) && SETUP_F32(AutogunRecord, unk88)
                   && SETUP_F32(AutogunRecord, unk8C) && SETUP_F32(MonitorObjRecord, damage)
                   && SETUP_F32(DoorRecord, maxFrac) && SETUP_F32(DoorRecord, perimFrac) && SETUP_F32(DoorRecord, accel)
                   && SETUP_F32(DoorRecord, decel) && SETUP_F32(DoorRecord, maxSpeed)
                   && SETUP_F32(struct BodyArmourRecord, initialamount) && SETUP_F32(struct TintedGlassRecord, unk90)
                   && SETUP_F32(struct CutsceneRecord, pos.f[0]) && SETUP_F32(struct CutsceneRecord, theta)
                   && SETUP_F32(struct CutsceneRecord, verta) && SETUP_F32(HatRecord, damage)
                   && SETUP_F32(WeaponObjRecord, damage),
               "the setup values converted in place are 4-byte f32 members");
/* initResolveAnimTable reads the animation bank's address as one intptr_t. */
_Static_assert(IS_TYPE(ptr_animation_table, struct animation_table_data *), "ptr_animation_table is read as one intptr_t");
/* projectileTestObjectCollision reads a model file header as its root node. */
_Static_assert(IS_TYPE(((ModelFileHeader *) 0)->RootNode, ModelNode *) && offsetof(ModelFileHeader, RootNode) == 0,
               "a model file header starts with its root node pointer");
/* gunfire.c and propobj.c write a switch slot's read-write record as one s32
 * without testing its opcode: each record those slots can hold that has a
 * scalar first is a 4-byte int. */
_Static_assert(IS_TYPE(((ModelRwData_SwitchRecord *) 0)->visible, s32) && IS_TYPE(((ModelRwData_LODRecord *) 0)->visible, s32)
                   && IS_TYPE(((ModelRwData_BSPRecord *) 0)->visible, s32)
                   && IS_TYPE(((ModelRwData_DisplayListRecord *) 0)->unk00, s32)
                   && IS_TYPE(((ModelRwData_Op07Record *) 0)->index, s32),
               "a switch, LOD, BSP, display-list and Op07 read-write record starts with a 4-byte int");
/* fileSetSaveCheatUnlocked sets a cheat bit at byte 14 + i, i 0 to 2. */
_Static_assert(offsetof(save_data, unlocked_cheats_1) == 0xe && offsetof(save_data, unlocked_cheats_3) == 0x10
                   && sizeof(((save_data *) 0)->unlocked_cheats_1) == 1 && sizeof(((save_data *) 0)->unlocked_cheats_3) == 1,
               "save_data's cheat bytes are bytes 14 to 16");
/* stan.c's walks end at a tile whose first word is 0: id and room fill it. */
_Static_assert(offsetof(StandTile, mid) == sizeof(u32) && sizeof(StandTile) == 8, "a stan tile's first word is its id and room");
#if IS_64_BIT
/* MODEL_SLOT_RWDATA (objecthandler.h) reads a slot's Model datas as a void *. */
_Static_assert(sizeof(((Model *) 0)->datas) == sizeof(void *), "a model slot's read-write data is the Model's datas pointer");
#endif

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
