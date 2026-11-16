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
POINTER_SIZED(Vertex, LinkedTo);
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
/* The host-pointer test (LP64-003): a segment word is never a host address;
 * at 64 bits a host address has one of its upper 32 bits set. */
_Static_assert(!PORT_IS_HOST_ADDR(0x0E000000u), "a segment-14 word is not a host address");
#if IS_64_BIT
_Static_assert(!PORT_IS_HOST_ADDR(0xFFFFFFFFu), "a 32-bit value is not a host address at 64 bits");
_Static_assert(PORT_IS_HOST_ADDR(0x100000000ull), "4 GB is a host address at 64 bits");
#endif
#if !IS_64_BIT
_Static_assert(sizeof(Gfx) == 8, "32-bit display-list stride");
_Static_assert(sizeof(fileentry) == 12, "ROM file table layout");
_Static_assert(sizeof(resource_lookup_data_entry) == 20, "ROM lookup layout");
/* Hilite's alignment member is `long int[4]` (include/PR/gbi.h), so it grows
 * from 16 to 32 bytes on LP64, where long is 8 bytes. */
_Static_assert(sizeof(Hilite) == 16, "32-bit highlight layout");
#endif

int main(void)
{
#if IS_64_BIT
    /* Reported, not asserted: model.c views an AnimModelSlot, and the hand its
     * field_B68, as a Model, which is larger at 64 bits (LP64-003). */
    printf("ABI probe: sizeof(struct AnimModelSlot) %d, sizeof(Model) %d\n",
           (int) sizeof(struct AnimModelSlot), (int) sizeof(Model));
#endif
    printf("ABI probe OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
