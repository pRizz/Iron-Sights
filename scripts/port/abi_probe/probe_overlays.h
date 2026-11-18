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
#include "game/chrai.h"
#include "game/lv.h"
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
/* MODEL_SLOT_RWDATA (objecthandler.h) reads a slot's Model datas as a void *:
 * datas must stay an object pointer, which a void * represents. */
_Static_assert(IS_TYPE(((Model *) 0)->datas, union ModelRwData **), "a model slot's read-write data is the Model's datas pointer");
#endif

/* Batch 3, the first half of the record sites (bg.c to options.c). */
_Static_assert(sizeof(coord2d) == 2 * sizeof(f32) && offsetof(coord2d, y) == sizeof(f32), "coord2d is two floats");
/* Room and model vertices are Vtx read through Vertex's coordinates. */
_Static_assert(sizeof(Vertex) == sizeof(Vtx) && offsetof(Vertex, coord.x) == offsetof(Vtx, v.ob[0])
                   && offsetof(Vertex, coord.y) == offsetof(Vtx, v.ob[1]) && offsetof(Vertex, coord.z) == offsetof(Vtx, v.ob[2])
                   && IS_TYPE(((Vertex *) 0)->coord.z, s16),
               "a Vertex's coordinates are a Vtx's three s16 at both widths");
/* The watch's sliders fill dyn Vtx as WatchVertex. */
_Static_assert(sizeof(struct WatchVertex) == sizeof(Vtx), "a WatchVertex is a Vtx's 16 bytes");
/* bgIsRoomOnScreen reads a bbox2d as a struct rectbbox. */
_Static_assert(sizeof(bbox2d) == sizeof(struct rectbbox) && sizeof(struct rectbbox) == 4 * sizeof(f32)
                   && offsetof(bbox2d, min.y) == offsetof(struct rectbbox, up)
                   && offsetof(bbox2d, max.x) == offsetof(struct rectbbox, right)
                   && offsetof(bbox2d, max.y) == offsetof(struct rectbbox, down),
               "bbox2d and struct rectbbox are four floats at both widths");
/* Setup commands: a BoundPadRecord read as a PadRecord (R4). */
_Static_assert(offsetof(BoundPadRecord, up) == offsetof(PadRecord, up) && offsetof(BoundPadRecord, look) == offsetof(PadRecord, look)
                   && offsetof(BoundPadRecord, plink) == offsetof(PadRecord, plink)
                   && offsetof(BoundPadRecord, stan) == offsetof(PadRecord, stan)
                   && offsetof(BoundPadRecord, bbox) == sizeof(PadRecord) && IS_TYPE(((BoundPadRecord *) 0)->pos, coord3d)
                   && IS_TYPE(((BoundPadRecord *) 0)->stan, StandTile *),
               "a BoundPadRecord starts with a PadRecord's members at both widths");
/* mp_respawn_handler reads an item intro record's flag through SetupIntroAmmo. */
_Static_assert(offsetof(struct SetupIntroAmmo, is_demo_playback) == offsetof(struct SetupIntroItem, is_demo_playback),
               "SetupIntroAmmo's is_demo_playback is SetupIntroItem's");
/* A monitor's script is a run of uintptr_t elements, read in threes. */
_Static_assert(sizeof(struct tvcmd) == 3 * sizeof(uintptr_t) && offsetof(struct tvcmd, time) == sizeof(uintptr_t)
                   && offsetof(struct tvcmd, arg2) == 2 * sizeof(uintptr_t),
               "a monitor script command is three script elements at both widths");
/* lvlTick allocates 0x3000 bytes for 3000 entries. */
_Static_assert(sizeof(struct LvlMpUnknown) == 4 && 3000 * sizeof(struct LvlMpUnknown) <= 0x3000,
               "a LvlMpUnknown is 4 bytes, so 3000 fit 0x3000");
/* An autogun's struct beam (0x30 bytes, setupAutogun) is read as a BeamRecord. */
_Static_assert(sizeof(struct beam) == sizeof(BeamRecord) && sizeof(BeamRecord) <= 0x30
                   && offsetof(struct beam, from) == offsetof(BeamRecord, pos)
                   && offsetof(struct beam, dir) == offsetof(BeamRecord, delta)
                   && offsetof(struct beam, maxdist) == offsetof(BeamRecord, unk1c)
                   && offsetof(struct beam, speed) == offsetof(BeamRecord, unk20)
                   && offsetof(struct beam, mindist) == offsetof(BeamRecord, unk24)
                   && offsetof(struct beam, dist) == offsetof(BeamRecord, unk28),
               "struct beam is a BeamRecord at both widths");
/* Three adjacent floats read as a coord3d. */
_Static_assert(offsetof(struct s_bullet_spark, unk14) == offsetof(struct s_bullet_spark, unk10) + sizeof(f32)
                   && offsetof(struct s_bullet_spark, unk18) == offsetof(struct s_bullet_spark, unk10) + 2 * sizeof(f32)
                   && IS_TYPE(((struct s_bullet_spark *) 0)->unk18, f32),
               "a bullet spark's unk10, unk14 and unk18 are three adjacent floats at both widths");
_Static_assert(offsetof(struct hand, field_8E0) == offsetof(struct hand, field_8DC) + sizeof(f32)
                   && offsetof(struct hand, field_8E4) == offsetof(struct hand, field_8DC) + 2 * sizeof(f32)
                   && IS_TYPE(((struct hand *) 0)->field_8E4, f32),
               "a hand's field_8DC, field_8E0 and field_8E4 are three adjacent floats at both widths");
_Static_assert(offsetof(Projectile, unkD8) == offsetof(Projectile, unkd4) + 4 && offsetof(Projectile, unkDC) == offsetof(Projectile, unkd4) + 8
                   && sizeof(((Projectile *) 0)->unkDC) == 4,
               "a Projectile's unkd4, unkD8 and unkDC are three adjacent 4-byte words at both widths");
/* gun.c's knife keyframes are u32 arrays read as Weapon1PTransformKeyframe. */
_Static_assert(sizeof(Weapon1PTransformKeyframe) == 9 * sizeof(u32) && offsetof(Weapon1PTransformKeyframe, pos) == sizeof(u32)
                   && offsetof(Weapon1PTransformKeyframe, rot) == 4 * sizeof(u32)
                   && offsetof(Weapon1PTransformKeyframe, interpParam) == 7 * sizeof(u32),
               "a Weapon1PTransformKeyframe is nine 4-byte words at both widths");
/* A rocket's ObjectRecord is used through the hand's AttachedObj view. */
_Static_assert(offsetof(AttachedObj, child) == offsetof(ObjectRecord, prop) && offsetof(AttachedObj, model) == offsetof(ObjectRecord, model)
                   && offsetof(AttachedObj, transform) == offsetof(ObjectRecord, mtx)
                   && offsetof(AttachedObj, position) == offsetof(ObjectRecord, runtime_pos)
                   && IS_TYPE(((ObjectRecord *) 0)->runtime_pos, coord3d),
               "an AttachedObj is an ObjectRecord's prop, model, mtx and runtime_pos at both widths");
/* A model root's header read-write record read as a modeldata_root. */
_Static_assert(sizeof(struct modeldata_root) == sizeof(ModelRwData_HeaderRecord)
                   && offsetof(struct modeldata_root, ground) == offsetof(ModelRwData_HeaderRecord, ground)
                   && offsetof(struct modeldata_root, pos) == offsetof(ModelRwData_HeaderRecord, pos)
                   && offsetof(struct modeldata_root, subroty) == offsetof(ModelRwData_HeaderRecord, unk14)
                   && offsetof(struct modeldata_root, unk34) == offsetof(ModelRwData_HeaderRecord, unk34)
                   && offsetof(struct modeldata_root, unk4c) == offsetof(ModelRwData_HeaderRecord, unk4c)
                   && offsetof(struct modeldata_root, unk58) == offsetof(ModelRwData_HeaderRecord, unk58)
                   && offsetof(struct modeldata_root, unk5c) == offsetof(ModelRwData_HeaderRecord, unk5c),
               "modeldata_root is a ModelRwData_HeaderRecord at both widths");
/* A switch slot array read as a ModelNode: ->Child is slot 5. */
_Static_assert(offsetof(ModelNode, Child) == 5 * sizeof(ModelNode *), "a ModelNode's Child is its sixth pointer-sized word");
/* Records defined in .c files, which the probe cannot see, are pinned from the
 * header side: their declared members' offsets, as the .c types lay them out. */
#define PTR_WORDS(n) ((n) * sizeof(void *))
_Static_assert(offsetof(struct HitThing, vtx0) == 6 * sizeof(f32) && offsetof(struct HitThing, vtx1) == 6 * sizeof(f32) + PTR_WORDS(1)
                   && offsetof(struct HitThing, vtx2) == 6 * sizeof(f32) + PTR_WORDS(2)
                   && offsetof(struct HitThing, tricmd) == 6 * sizeof(f32) + PTR_WORDS(3)
                   && offsetof(struct HitThing, unk28) == 6 * sizeof(f32) + PTR_WORDS(4)
                   && offsetof(struct HitThing, texturenum) == 6 * sizeof(f32) + PTR_WORDS(4) + sizeof(s16)
                   && IS_TYPE(((struct HitThing *) 0)->normal, coord3d),
               "HitThing's members to texturenum are at the offsets bg.c's HitThingSub declares");
_Static_assert(offsetof(Model, rwdatalen) == sizeof(s16) && offsetof(Model, chr) == PTR_WORDS(1) && offsetof(Model, obj) == PTR_WORDS(2)
                   && offsetof(Model, render_pos) == PTR_WORDS(3) && offsetof(Model, datas) == PTR_WORDS(4)
                   && offsetof(Model, scale) == PTR_WORDS(5) && offsetof(Model, attachedto) == PTR_WORDS(6)
                   && offsetof(Model, attachedto_objinst) == PTR_WORDS(7),
               "Model's first nine members are at the offsets gunfire.c's ModelHeader and ModelHead declare");
#undef PTR_WORDS

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
