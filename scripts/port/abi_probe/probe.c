/* Compile-time layout checks for the structs the renderer and ROM loader
 * depend on. Built at both pointer widths by scripts/port/test_abi_probe.sh. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ultra64.h>
#include <PR/abi.h>
#include "ob.h"
#include "platform_info.h"
#include <bondtypes.h>
#include "sched.h"
#include "game/bg.h"
#include "game/objecthandler.h"
#include "game/vtxstore.h"
#include "game/bondview.h"
#include "port_hostptr.h"

/* Fixed at every width: the renderer reads these as N64 formats. */
_Static_assert(sizeof(s32) == 4 && sizeof(u32) == 4, "32-bit integer types");
_Static_assert(sizeof(Vtx) == 16, "Vertex stride");
_Static_assert(sizeof(Mtx) == 64, "Matrix layout");
_Static_assert(sizeof(Vp) == 16, "Viewport layout");
_Static_assert(sizeof(Acmd) == 8, "Audio command stride");
_Static_assert(sizeof(Light_t) == 12 && sizeof(Light) == 16, "Directional light layout");
_Static_assert(sizeof(Ambient_t) == 8 && sizeof(Ambient) == 8, "Ambient light layout");
_Static_assert(sizeof(Lights0) == 24 && sizeof(Lights1) == 24 && sizeof(Lights2) == 40
               && sizeof(Lights3) == 56 && sizeof(Lights4) == 72 && sizeof(Lights5) == 88
               && sizeof(Lights6) == 104 && sizeof(Lights7) == 120 && sizeof(Lightsn) == 120,
               "Light list layouts");
_Static_assert(sizeof(LookAt) == 32, "Look-at layout");
_Static_assert(sizeof(Hilite_t) == 16, "Highlight offsets layout");
/* gbi.h declared these alignment and command words `long`, which is 8 bytes
 * on LP64; they are 32-bit words at every width (LP64-003). */
_Static_assert(sizeof(Hilite) == 16, "Highlight layout");
_Static_assert(sizeof(Gsetcolor) == 8, "Set-colour command layout");
_Static_assert(sizeof(TexRect) == 16, "Textured rectangle layout");
/* The game's Vertex is laid out as a Vtx; at 64 bits its +8 link is a 32-bit
 * offset from the model file's base (bondtypes.h, LP64-003). */
_Static_assert(sizeof(Vertex) == 16, "Game vertex stride");
/* Scales with the pointer. */
_Static_assert(IS_64_BIT == (sizeof(void *) == 8), "IS_64_BIT matches the host");
_Static_assert(sizeof(Gfx) == 2 * sizeof(uintptr_t), "Display-list command is two pointer words");
_Static_assert(offsetof(Gfx, words.w1) == sizeof(uintptr_t), "Display-list address word");
/* Fields behind the LP64-002 sentinel comparisons (a small integer, a mask or
 * a segment tag tested in pointer-sized storage): each must hold a whole
 * pointer at every width. How they behave at 64 bits is LP64-003's to test. */
#define POINTER_SIZED(type, member) \
    _Static_assert(sizeof(((type *) 0)->member) == sizeof(void *), #type "." #member " is pointer-sized")
POINTER_SIZED(ModelFileTextures, TextureID);        /* < MAX_TEXTURES: gun.c, objecthandler_2.c, image.c */
POINTER_SIZED(sImageTableEntry, index);             /* < MAX_TEXTURES - 1: front.c */
POINTER_SIZED(MonitorRecord, tconfig);              /* < 100: propobj.c */
POINTER_SIZED(ModelAnimation, address);             /* & 0x80000000: model.c */
POINTER_SIZED(bg_portal_data_entry, offset_portal); /* BG_SEG_IS_OFFSET: bg.c */
POINTER_SIZED(OSScClient, next);                    /* next = 1: audi.c */
POINTER_SIZED(Gfx, words.w1);                       /* == 0x0E000000: lightfixture.c */
/* The three slots that hold a number or a whole pointer and are tested
 * against a small constant (LP64-003 Task 6): unsigned as well as
 * pointer-sized, so the full-width comparisons stay unsigned. */
#define UNSIGNED_FIELD(type, member) \
    _Static_assert((__typeof__(((type *) 0)->member)) -1 > 0, #type "." #member " is unsigned")
UNSIGNED_FIELD(ModelFileTextures, TextureID);
UNSIGNED_FIELD(sImageTableEntry, index);
UNSIGNED_FIELD(Gfx, words.w1);
/* Every field modelPromoteNodeOffsetsToPointers (model.c) PROMOTEs, which
 * tests the 0x05000000 segment tag. PROMOTE casts through __typeof__, so on
 * an integer field (Op11's unk0c) only this assertion sees a narrowing. */
POINTER_SIZED(ModelNode, Data);
POINTER_SIZED(ModelNode, Parent);
POINTER_SIZED(ModelNode, Next);
POINTER_SIZED(ModelNode, Prev);
POINTER_SIZED(ModelNode, Child);
POINTER_SIZED(ModelRoData_HeaderRecord, FirstGroup);
POINTER_SIZED(ModelRoData_GroupRecord, ChildGroup);
POINTER_SIZED(ModelRoData_DisplayListRecord, Primary);
POINTER_SIZED(ModelRoData_DisplayListRecord, Secondary);
POINTER_SIZED(ModelRoData_DisplayListRecord, Vertices);
POINTER_SIZED(ModelRoData_DisplayList_CollisionRecord, Primary);
POINTER_SIZED(ModelRoData_DisplayList_CollisionRecord, Secondary);
POINTER_SIZED(ModelRoData_DisplayList_CollisionRecord, Vertices);
POINTER_SIZED(ModelRoData_DisplayList_CollisionRecord, CollisionVertices);
POINTER_SIZED(ModelRoData_DisplayList_CollisionRecord, PointUsage);
#if IS_64_BIT
/* At 64 bits the collision link is a 32-bit offset (bondtypes.h), which
 * modelPromoteNodeOffsetsToPointers does not PROMOTE. */
_Static_assert(sizeof(((Vertex *) 0)->LinkOffset) == 4 && offsetof(Vertex, LinkOffset) == 8,
               "Vertex.LinkOffset is the 32-bit slot at 0x8");
#else
POINTER_SIZED(Vertex, LinkedTo);
#endif
POINTER_SIZED(ModelRoData_Op05Record, Children);
POINTER_SIZED(ModelRoData_Op05Record, Vertices);
POINTER_SIZED(ModelRoData_Op05Record, Images);
POINTER_SIZED(ModelRoData_Child, unk04);
POINTER_SIZED(ModelRoData_Op07Record, unk00);
POINTER_SIZED(ModelRoData_Op07Record, unk04);
POINTER_SIZED(ModelRoData_Op07Record, Children);
POINTER_SIZED(ModelRoData_Op07Record, Vertices);
POINTER_SIZED(ModelRoData_Op07Record, Images);
POINTER_SIZED(ModelRoData_LODRecord, Affects);
POINTER_SIZED(ModelRoData_SwitchRecord, Controls);
POINTER_SIZED(ModelRoData_BSPRecord, leftChild);
POINTER_SIZED(ModelRoData_BSPRecord, rightChild);
POINTER_SIZED(ModelRoData_Op11Record, unk0c[15]);
POINTER_SIZED(ModelRoData_GunfireRecord, Image);
POINTER_SIZED(ModelRoData_ShadowRecord, image);
POINTER_SIZED(ModelRoData_ShadowRecord, Header);
POINTER_SIZED(ModelRoData_DisplayListPrimaryRecord, Vertices);
POINTER_SIZED(ModelRoData_DisplayListPrimaryRecord, Primary);
/* bondview2.c passes this as the collision walk's record, which
 * stanTileDistanceRelated clears and its callbacks write by name (LP64-003). */
_Static_assert(sizeof(struct move_bond_temp_struct) >= sizeof(struct StandTileLocusCallbackRecord)
               && _Alignof(struct move_bond_temp_struct) >= _Alignof(struct StandTileLocusCallbackRecord),
               "movement locus storage holds a collision walk record");
/* The host-pointer test (LP64-003): a segment word is never a host address;
 * at 64 bits a host address has one of its upper 32 bits set. */
_Static_assert(!PORT_IS_HOST_ADDR(0x0E000000u), "a segment-14 word is not a host address");
#if IS_64_BIT
_Static_assert(!PORT_IS_HOST_ADDR(0xFFFFFFFFu), "a 32-bit value is not a host address at 64 bits");
_Static_assert(PORT_IS_HOST_ADDR(0x100000000ull), "4 GB is a host address at 64 bits");
#endif
/* Model storage (LP64-003): model.c hands out slots as Models, gunfire.c
 * builds the gun's Model in the hand from field_B68 with its read-write data
 * from modeldatas (32 slots), and bondview2.c the pause watch's in the player
 * from something_with_watch_object_instance with its data from field_2EC (50
 * slots). At 32 bits the N64 layout stays; the slot fields alias the Model
 * members MODEL_SLOT_OBJ and MODEL_SLOT_RWDATA name. */
#define OFFSET(type, member, at) _Static_assert(offsetof(type, member) == (at), #type "." #member " offset")
#if IS_64_BIT
_Static_assert(sizeof(struct ModelSlot) >= sizeof(Model) && sizeof(struct AnimModelSlot) >= sizeof(Model),
               "model slots hold a Model");
OFFSET(struct ModelSlot, model, 0);
OFFSET(struct AnimModelSlot, model, 0);
OFFSET(struct ModelSlot, unk02, offsetof(Model, rwdatalen));
OFFSET(struct AnimModelSlot, unk02, offsetof(Model, rwdatalen));
_Static_assert(offsetof(struct hand, field_B68_model) == offsetof(struct hand, field_B68)
               && offsetof(struct hand, modeldatas) - offsetof(struct hand, field_B68) >= sizeof(Model),
               "the hand's gun model run holds a Model");
_Static_assert(offsetof(struct hand, modeldatas_slots) == offsetof(struct hand, modeldatas)
               && offsetof(struct hand, volley) - offsetof(struct hand, modeldatas) >= 32 * sizeof(void *),
               "the hand's read-write run holds 32 slots");
_Static_assert(offsetof(struct player, watch_model) == offsetof(struct player, something_with_watch_object_instance)
               && offsetof(struct player, field_2EC) - offsetof(struct player, something_with_watch_object_instance)
                      >= sizeof(Model),
               "the player's watch model run holds a Model");
_Static_assert(offsetof(struct player, watch_rwdata_slots) == offsetof(struct player, field_2EC)
               && offsetof(struct player, buttons_pressed) - offsetof(struct player, field_2EC) >= 50 * sizeof(void *),
               "the player's watch read-write run holds 50 slots");
#else
_Static_assert(sizeof(Model) == 0xbc, "32-bit Model layout");
OFFSET(Model, rwdatalen, 0x02);
OFFSET(Model, obj, 0x08);
OFFSET(Model, datas, 0x10);
_Static_assert(sizeof(struct ModelSlot) == 0x20, "32-bit model slot layout");
OFFSET(struct ModelSlot, unk02, 0x02);
OFFSET(struct ModelSlot, unk04, 0x04);
OFFSET(struct ModelSlot, unk08, 0x08);
OFFSET(struct ModelSlot, unk0c, 0x0c);
OFFSET(struct ModelSlot, unk10, 0x10);
OFFSET(struct ModelSlot, unk14, 0x14);
OFFSET(struct ModelSlot, unk1c, 0x1c);
/* Every field from unk14 to unkb8 is an s32, so these place all of them. */
_Static_assert(sizeof(struct AnimModelSlot) == 0xbc, "32-bit animated model slot layout");
OFFSET(struct AnimModelSlot, unk02, 0x02);
OFFSET(struct AnimModelSlot, unk04, 0x04);
OFFSET(struct AnimModelSlot, unk08, 0x08);
OFFSET(struct AnimModelSlot, unk0c, 0x0c);
OFFSET(struct AnimModelSlot, unk10, 0x10);
OFFSET(struct AnimModelSlot, unk14, 0x14);
OFFSET(struct AnimModelSlot, unkb8, 0xb8);
/* The hand's fields from field_B68 to volley are s32 apart from mtxlist. */
_Static_assert(sizeof(struct hand) == 0x3a8, "32-bit hand layout");
OFFSET(struct hand, field_B68, 0x2f8);
OFFSET(struct hand, mtxlist, 0x304);
OFFSET(struct hand, modeldatas, 0x318);
OFFSET(struct hand, field_C04, 0x394);
OFFSET(struct hand, volley, 0x398);
/* bondview2.c's raw offsets 0x220, 0x230 and 0x2ec, and the fields that
 * alias the watch Model's render_pos, scale and animframe1. */
_Static_assert(sizeof(struct player) == 0x2a80, "32-bit player layout");
OFFSET(struct player, step_in_view_watch_animation, 0x220);
OFFSET(struct player, something_with_watch_object_instance, 0x230);
OFFSET(struct player, field_23C, 0x230 + 0x0c);
OFFSET(struct player, watch_scale_destination, 0x230 + 0x14);
OFFSET(struct player, pause_watch_related_adjust, 0x230 + 0x28);
OFFSET(struct player, field_2EC, 0x2ec);
OFFSET(struct player, field_3B0, 0x3b0);
OFFSET(struct player, buttons_pressed, 0x3b4);
#endif
#if !IS_64_BIT
_Static_assert(sizeof(Gfx) == 8, "32-bit display-list stride");
_Static_assert(sizeof(fileentry) == 12, "ROM file table layout");
_Static_assert(sizeof(resource_lookup_data_entry) == 20, "ROM lookup layout");
/* vtxstore.c allocates its slot arrays by this record's size, which was the
 * literal 0x14 before LP64-003 (32 bytes at 64 bits). */
_Static_assert(sizeof(struct unk_09B7A0_struct_parent) == 0x14, "32-bit vertex store slot layout");
#endif

int main(void)
{
    printf("ABI probe OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
