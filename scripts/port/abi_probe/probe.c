/* Compile-time layout checks for the structs the renderer and ROM loader
 * depend on. Built at both pointer widths by scripts/port/test_abi_probe.sh. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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
#include "game/model.h"
#include "game/glass.h"
#include "port_hostptr.h"
#include "game/ramromreplay.h"
#include "port_romlayout.h"
#include "snd.h"
#include "probe_overlays.h"
#include "probe_stored.h"
#include "probe_views.h"

/* Fixed at every width: the renderer reads these as N64 formats. */
_Static_assert(sizeof(s32) == 4 && sizeof(u32) == 4, "32-bit integer types");
_Static_assert(sizeof(Vtx) == 16, "Vertex stride");
_Static_assert(sizeof(Mtx) == 64, "Matrix layout");
_Static_assert(sizeof(Vp) == 16, "Viewport layout");
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
/* An audio command carries a buffer address in w1 (abi.h, LP64-004). */
_Static_assert(sizeof(Acmd) == 2 * sizeof(void *), "Audio command is two pointer words");
/* sndPlaySfx reads the effects bank's instrument through ALInstrumentAlt_s
 * with 1-based sound indices: its index k must be ALInstrument's k - 1. */
_Static_assert(offsetof(struct ALInstrumentAlt_s, soundArray) + sizeof(void *) == offsetof(ALInstrument, soundArray),
               "ALInstrumentAlt_s's sound array starts one pointer before ALInstrument's");
/* g_musicSfxBufferPtr, an ALBank *, is passed to sndPlaySfx as an ALBankAlt_s *. */
_Static_assert(offsetof(struct ALBankAlt_s, instArray) == offsetof(ALBank, instArray), "ALBankAlt_s's instrument array");
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

/* The model converter (romconv_model_records.c) writes a bounding box's floats
 * at their file places at both widths: it keeps its model number first, so its
 * floats start at 4. */
_Static_assert(offsetof(ModelRoData_BoundingBoxRecord, Bounds) == 4, "a bounding box's floats follow its model number");
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
OFFSET(struct player, gait_rwdata_slots, offsetof(struct player, field_654));
_Static_assert(offsetof(struct player, gait_model_storage) == offsetof(struct player, model)
                   && offsetof(struct player, field_654) - offsetof(struct player, model) >= sizeof(Model),
               "the player's gait model run holds a Model");
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
/* These named fields are the watch's and the gun's Model members at 32 bits;
 * the WATCH_MODEL_* and GUN_MODEL_* names in bondview.h read them through the
 * Model at 64 bits. */
#define MODEL_ALIAS(type, base, field, member)                                                            \
    _Static_assert(offsetof(type, field) - offsetof(type, base) == offsetof(Model, member),             \
                   #type "." #field " is " #base "'s Model." #member)
MODEL_ALIAS(struct player, something_with_watch_object_instance, field_23C, render_pos);
MODEL_ALIAS(struct player, something_with_watch_object_instance, watch_scale_destination, scale);
MODEL_ALIAS(struct player, something_with_watch_object_instance, pause_watch_related_adjust, animframe1);
MODEL_ALIAS(struct hand, field_B68, mtxlist, render_pos);
#endif
/* The player's gait model run (bondview.h, model to field_650): at 32 bits
 * each named word is the Model field it overlays. At 64 bits the Model is
 * larger and the names land elsewhere, so each is SAME (the names still meet
 * at 64 bits), BRANCHED (every reader takes gait_model_storage's field at 64
 * bits: bondhead.c) or UNREAD (no code names it; test_abi_probe.sh fails on a
 * reader). The 64-bit assertion fails for any name marked SAME that is not. */
/* The number of GAIT_RUN lines below; test_abi_probe.sh fails unless it
 * reads exactly this many, so a line it cannot parse does not drop out. */
#define GAIT_RUN_NAMES 50
#define GAIT_SAME 0
#define GAIT_BRANCHED 1
#define GAIT_UNREAD 2
#define GAIT_OFF(name) (offsetof(struct player, name) - offsetof(struct player, model))
#if IS_64_BIT
#define GAIT_RUN(name, field, how) \
    _Static_assert((how) != GAIT_SAME || GAIT_OFF(name) == offsetof(Model, field), "gait run " #name " is Model." #field)
#else
#define GAIT_RUN(name, field, how) \
    _Static_assert(GAIT_OFF(name) == offsetof(Model, field), "gait run " #name " is Model." #field)
#endif
GAIT_RUN(model, unk00, GAIT_SAME);
GAIT_RUN(field_59C, chr, GAIT_SAME);
GAIT_RUN(field_5A0, obj, GAIT_UNREAD);
GAIT_RUN(field_5A4, render_pos, GAIT_UNREAD);
GAIT_RUN(field_5A8, datas, GAIT_UNREAD);
GAIT_RUN(field_5AC, scale, GAIT_UNREAD);
GAIT_RUN(field_5B0, attachedto, GAIT_UNREAD);
GAIT_RUN(field_5B4, attachedto_objinst, GAIT_UNREAD);
GAIT_RUN(field_5B8, anim, GAIT_UNREAD);
GAIT_RUN(animFlipFlag, gunhand, GAIT_BRANCHED);
GAIT_RUN(field_5BD, unk25, GAIT_UNREAD);
GAIT_RUN(field_5BE, animlooping, GAIT_UNREAD);
GAIT_RUN(field_5BF, unk27, GAIT_UNREAD);
GAIT_RUN(field_5C0, animframe1, GAIT_BRANCHED);
GAIT_RUN(field_5C4, unk2c, GAIT_UNREAD);
GAIT_RUN(field_5C8, framea, GAIT_UNREAD);
GAIT_RUN(field_5CC, unk34, GAIT_UNREAD);
GAIT_RUN(field_5D0, unk38, GAIT_UNREAD);
GAIT_RUN(field_5D4, endframe, GAIT_UNREAD);
GAIT_RUN(field_5D8, speed, GAIT_UNREAD);
GAIT_RUN(field_5DC, newspeed, GAIT_UNREAD);
GAIT_RUN(field_5E0, oldspeed, GAIT_UNREAD);
GAIT_RUN(field_5E4, timespeed, GAIT_UNREAD);
GAIT_RUN(field_5E8, elapsespeed, GAIT_UNREAD);
GAIT_RUN(field_5EC, anim2, GAIT_UNREAD);
GAIT_RUN(field_5F0, animframe2, GAIT_UNREAD);
GAIT_RUN(field_5F4, unk5c, GAIT_UNREAD);
GAIT_RUN(field_5F8, frame2a, GAIT_UNREAD);
GAIT_RUN(field_5FC, unk64, GAIT_UNREAD);
GAIT_RUN(field_600, unk68, GAIT_UNREAD);
GAIT_RUN(field_604, unk6c, GAIT_UNREAD);
GAIT_RUN(field_608, speed2, GAIT_UNREAD);
GAIT_RUN(field_60C, unk74, GAIT_UNREAD);
GAIT_RUN(field_610, unk78, GAIT_UNREAD);
GAIT_RUN(field_614, unk7c, GAIT_UNREAD);
GAIT_RUN(field_618, unk80, GAIT_UNREAD);
GAIT_RUN(field_61C, unk84, GAIT_UNREAD);
GAIT_RUN(field_620, unk88, GAIT_UNREAD);
GAIT_RUN(field_624, unk8c, GAIT_UNREAD);
GAIT_RUN(field_628, animloopframe, GAIT_UNREAD);
GAIT_RUN(field_62C, animloopmerge, GAIT_UNREAD);
GAIT_RUN(field_630, animflipfunc, GAIT_UNREAD);
GAIT_RUN(field_634, unk9c, GAIT_UNREAD);
GAIT_RUN(field_638, unka0, GAIT_UNREAD);
GAIT_RUN(field_63C, playspeed, GAIT_UNREAD);
GAIT_RUN(field_640, animrate, GAIT_UNREAD);
GAIT_RUN(field_644, unkac, GAIT_UNREAD);
GAIT_RUN(field_648, unkb0, GAIT_UNREAD);
GAIT_RUN(field_64C, unkb4, GAIT_UNREAD);
GAIT_RUN(field_650, anim_translation_scale, GAIT_UNREAD);
/* Model read-write data (LP64-003 final review): a record covers
 * MODEL_RWDATA_UNITS slots of Model.datas, and no record takes more slots at
 * 64 bits than at 32, so the runs sized for the N64's counts hold every model
 * once their slots are pointer-sized. */
#define RWDATA_UNITS(record, at32, at64) \
    _Static_assert(MODEL_RWDATA_UNITS(struct record) == (IS_64_BIT ? (at64) : (at32)), #record " read-write slots")
RWDATA_UNITS(ModelRwData_HeaderRecord, 24, 12);
RWDATA_UNITS(ModelRwData_Op07Record, 1, 1);
RWDATA_UNITS(ModelRwData_LODRecord, 1, 1);
RWDATA_UNITS(ModelRwData_SwitchRecord, 1, 1);
RWDATA_UNITS(ModelRwData_HeadPlaceholderRecord, 2, 2);
RWDATA_UNITS(ModelRwData_BSPRecord, 1, 1);
RWDATA_UNITS(ModelRwData_Op11Record, 1, 1);
RWDATA_UNITS(ModelRwData_GunfireRecord, 1, 1);
RWDATA_UNITS(ModelRwData_DisplayList_CollisionRecord, 2, 2);
_Static_assert(MODEL_RWDATA_BYTES(1) == sizeof(union ModelRwData *), "a read-write slot's bytes");
_Static_assert(MODEL_NODE_SLOT_BYTES(1) == sizeof(ModelNode *), "a node-array entry's bytes");
_Static_assert(MODEL_STORAGE_PAD == sizeof(Model) + 0x3f, "bondview2.c's Model step in weaponbuf0");
/* initBondDATAdefaults.c gives the player's gait model field_654 to
 * field_6CC as its read-write data: 31 slots (it complains at 31 records). */
_Static_assert(offsetof(struct player, bondheadmatrices) - offsetof(struct player, field_654)
                   >= 31 * sizeof(union ModelRwData *),
               "the player's gait read-write run holds 31 slots");
/* glass2.c's bullet spark steps its image table by the element and reads its
 * colour by name (LP64-003 final review): the entry is 12 bytes at 32 bits and
 * 16 at 64, and the colour moves with the pointer before it. */
_Static_assert(sizeof(struct sImageTableEntry) == (IS_64_BIT ? 16 : 12), "image table entry stride");
#if !IS_64_BIT
OFFSET(s_bullet_spark, unk28, 0x28);
OFFSET(s_bullet_spark, unk2B, 0x2b);
#endif
#if !IS_64_BIT
/* gunfire.c's render templates are 16 words, one per member in this order
 * (port_model_words.h maps them by member at 64 bits). */
_Static_assert(sizeof(ModelRenderData) == 0x40, "32-bit render data layout");
OFFSET(ModelRenderData, zbufferenabled, 0x04);
OFFSET(ModelRenderData, flags, 0x08);
OFFSET(ModelRenderData, gdl, 0x0c);
OFFSET(ModelRenderData, mtxlist, 0x10);
OFFSET(ModelRenderData, unk14, 0x14);
OFFSET(ModelRenderData, unk2c, 0x2c);
OFFSET(ModelRenderData, PropType, 0x30);
OFFSET(ModelRenderData, envcolour, 0x34);
OFFSET(ModelRenderData, fogcolour, 0x38);
OFFSET(ModelRenderData, cullmode, 0x3c);
/* gunfire.c's casing code adds these to the player plus a hand's offset:
 * the hand's throw matrix and the position rows of it and its previous. */
OFFSET(struct player, hands[0].throw_item_pos_related, 0xad8);
OFFSET(struct player, hands[0].throw_item_pos_related.m[3], 0xb08);
OFFSET(struct player, hands[0].throw_item_pos_related_prev.m[3], 0xb48);
#endif
#if !IS_64_BIT
_Static_assert(sizeof(Gfx) == 8, "32-bit display-list stride");
_Static_assert(sizeof(fileentry) == 12, "ROM file table layout");
_Static_assert(sizeof(resource_lookup_data_entry) == 20, "ROM lookup layout");
/* vtxstore.c allocates its slot arrays by this record's size, which was the
 * literal 0x14 before LP64-003 (32 bytes at 64 bits). */
_Static_assert(sizeof(struct unk_09B7A0_struct_parent) == 0x14, "32-bit vertex store slot layout");
#endif

/* The attract demo header (LP64-004 Task 1). The cart reserves 0xE8 bytes for
 * it; the game's struct is 0xE8 bytes where a u64 is 8-byte aligned in a struct
 * (the N64, MinGW, every 64-bit build) and 228 on i686 Linux, with the same
 * field offsets everywhere. Playback steps by RAMROM_HEADER_ROMSIZE (0xE8). */
_Static_assert(sizeof(RomDemoHeader) == 0xE8, "demo header file layout");
#if IS_64_BIT
_Static_assert(sizeof(struct ramromfilestructure) == 0xE8, "64-bit demo header is the cart's size");
#else
_Static_assert(sizeof(struct ramromfilestructure) == 228, "i686 Linux demo header is 4 bytes short of the cart's");
#endif
#define SAME_OFFSET(game, rom) \
    _Static_assert(offsetof(struct ramromfilestructure, game) == offsetof(RomDemoHeader, rom), \
                   "demo header " #game " is at its file offset")
SAME_OFFSET(randomizer, randomizer);
SAME_OFFSET(stagenum, stagenum);
SAME_OFFSET(savefile, save_chksum1);
SAME_OFFSET(savefile.times, save_times);
SAME_OFFSET(totaltime_ms, totaltime_ms);
SAME_OFFSET(mp_weapon_set, mp_weapon_set);
SAME_OFFSET(aim_option, aim_option);
SAME_OFFSET(mp_flags, mp_flags);
_Static_assert(sizeof(RomBrief) == 48, "briefing file layout");

/* The animation header (LP64-004 Task 3): 0x14 bytes in the file. At 64 bits
 * the run-time ModelAnimation is written over it, packed and 4-byte aligned
 * so that its read fields end at 0x20; at 32 bits it is unchanged. */
_Static_assert(sizeof(RomAnimHeader) == 0x14, "animation header file layout");
#define ANIM_AT(member, at32, at64) \
    _Static_assert(offsetof(ModelAnimation, member) == (IS_64_BIT ? (at64) : (at32)), "ModelAnimation." #member)
ANIM_AT(address, 0x00, 0);
ANIM_AT(unk04, 0x04, 8);
ANIM_AT(bitDescriptors, 0x08, 12);
ANIM_AT(unk0C, 0x0C, 20);
ANIM_AT(unk0E, 0x0E, 22);
ANIM_AT(bitStream, 0x10, 24);
ANIM_AT(unk14, 0x14, 32);
#if IS_64_BIT
_Static_assert(_Alignof(ModelAnimation) == 4, "64-bit ModelAnimation is 4-byte aligned");
#else
_Static_assert(sizeof(ModelAnimation) == 0x40, "32-bit ModelAnimation is unchanged");
#endif

/* Reads by position that LP64-005 Task 4 made reads by name: at 32 bits each
 * member is the bytes the old read reached, so the 32-bit program keeps its
 * loads and stores (compare_win32.py shows it); at 64 bits the member moved. */
#if !IS_64_BIT
_Static_assert(offsetof(WeaponObjRecord, weaponnum) == 0x80 && sizeof(((WeaponObjRecord *) 0)->weaponnum) == 1
                   && offsetof(ChrRecord, act_bytes.padding[84]) == 0x80,
               "a weapon's weaponnum is the byte ChrRecord.act_bytes.padding[84] read at 32 bits");
_Static_assert(offsetof(ChrRecord, act_gopos.unk59) == offsetof(ChrRecord, act_ubytes.padding[45])
                   && sizeof(((ChrRecord *) 0)->act_gopos.unk59) == 1,
               "act_gopos.unk59 is the byte ChrRecord.act_ubytes.padding[45] read at 32 bits");
_Static_assert(offsetof(struct weapon_firing_animation_table, max_up) == 12 * sizeof(f32)
                   && offsetof(struct weapon_firing_animation_table, max_down) == 13 * sizeof(f32)
                   && offsetof(struct weapon_firing_animation_table, free_arm_frac_up) == 16 * sizeof(f32)
                   && offsetof(struct weapon_firing_animation_table, free_arm_frac_down) == 17 * sizeof(f32),
               "a firing table's max_up, max_down and free arm fractions are its f32 words 12, 13, 16 and 17 at 32 bits");
_Static_assert(offsetof(ModelAnimation, unk04) == 2 * sizeof(u16) && IS_TYPE(((ModelAnimation *) 0)->unk04, u16),
               "an animation's frame count unk04 is its u16 halfword 2 at 32 bits");
_Static_assert(4 * sizeof(Gfx) + offsetof(Gfx, words.w0) == 8 * sizeof(u32) && sizeof(((Gfx *) 0)->words.w0) == sizeof(u32),
               "a display list's fifth command's w0 is its u32 word 8 at 32 bits");
#endif

/* objecthandler.c sizes its ModelHitEntry pool (600 records, threaded by
 * initModelHitEntryFreeList) by the record: 0x14 bytes at 32 bits, as the
 * literal it replaced, and 40 at 64. */
_Static_assert(sizeof(ModelHitEntry) == (IS_64_BIT ? 40 : 0x14), "model hit entry record");
/* inititemslots.c sizes the inventory by the record: 0x14 bytes at 32 bits,
 * the N64 literal it replaced, and 32 at 64 bits (LP64-004). */
_Static_assert(sizeof(struct InvItem) == (IS_64_BIT ? 32 : 0x14), "inventory item record");
/* player.c allocates the player by the record at 64 bits; the 32-bit build
 * keeps the N64 literal, 0x2a80 (asserted with the player layout above). */
_Static_assert(!IS_64_BIT || sizeof(struct player) > 0x2a80, "the 64-bit player outgrows the N64 literal");
/* The pause code's display lists in the player (bondview2.c, strict tier,
 * read for these counts; LP64-004 fix wave): each room runs from its first
 * member to the next named one, and holds the commands its writer emits at
 * either width. A writer that emits more must raise its count here; the
 * backdrop's count follows WATCH_NUMBER_SCREENS and the bars' is counted at
 * run time from buildGaugeBarDL's loop (checkGaugeBarRooms). */
#define PLAYER_DL_ROOM(first, next) ((offsetof(struct player, next) - offsetof(struct player, first)) / sizeof(Gfx))
#define PLAYER_DL_ALIGNED(first) (offsetof(struct player, first) % _Alignof(Gfx) == 0)
/* The watch static: sub_GAME_7F0A3B40's two commands and the end, once. */
#define WATCH_STATIC_DL_COMMANDS 3
/* The screen select rectangles: the same two for each screen, and the end. */
#define WATCH_BACKDROP_DL_COMMANDS (2 * WATCH_NUMBER_SCREENS + 1)
_Static_assert(PLAYER_DL_ROOM(buffer_for_watch_static_DL, healthdamagetype) >= WATCH_STATIC_DL_COMMANDS,
               "the watch static list's room");
_Static_assert(PLAYER_DL_ROOM(buffer_for_watch_greenbackdrop_DL, buffer_for_watch_static_DL)
                   >= WATCH_BACKDROP_DL_COMMANDS,
               "the watch backdrop list's room");
_Static_assert(PLAYER_DL_ALIGNED(buffer_for_watch_static_DL) && PLAYER_DL_ALIGNED(buffer_for_watch_greenbackdrop_DL),
               "the watch lists are Gfx-aligned");
/* initunk_005520.c's anim model slots: the array stride is the record, which
 * must keep the bank's 8-byte alignment at 64 bits (0xBC at 32 bits, whose
 * 0xC0 allocation stride keeps 16). */
_Static_assert(IS_64_BIT ? sizeof(struct AnimModelSlot) % 8 == 0 : sizeof(struct AnimModelSlot) == 0xbc,
               "anim model slot record");

/* The font table (LP64-004 Task 9): 0xB74 bytes in the file. struct fontchar
 * keeps the pixel offset in a pointer slot, so at 64 bits struct font is 0x2F4
 * bytes longer (segments.c's staging arrays grow by that, rounded up to 0x300
 * to keep the stage bank 16-aligned); at 32 it is the
 * file's table. */
_Static_assert(sizeof(RomFont) == 0xB74, "font table file layout");
_Static_assert(sizeof(struct font) - sizeof(RomFont) == (IS_64_BIT ? 0x2F4 : 0), "struct font's growth");
_Static_assert(sizeof(struct fontchar) == (IS_64_BIT ? 32 : 24), "fontchar stride");

/* The setup file's run-time records (LP64-004 Task 14): the converter writes
 * each at its C size, the sizes the plan measured at both widths. The intro
 * camera's text IDs sit at the start of their unions, its prev link after
 * them, and the credits record's table offset is pointer-sized at 64 bits. */
#define SETUP_SIZE(type, at32, at64) \
    _Static_assert(sizeof(type) == (IS_64_BIT ? (at64) : (at32)), #type " size")
SETUP_SIZE(struct stagesetup, 40, 80);
SETUP_SIZE(PadRecord, 44, 56);
SETUP_SIZE(BoundPadRecord, 68, 80);
SETUP_SIZE(waypoint, 16, 24);
SETUP_SIZE(waygroup, 12, 24);
SETUP_SIZE(PathRecord, 8, 16);
SETUP_SIZE(AIListRecord, 8, 16);
SETUP_SIZE(pname, 4, 8);
SETUP_SIZE(SetupIntroCamera, 40, 56);
SETUP_SIZE(struct SetupIntroCredits, 8, 16);
SETUP_SIZE(CreditsEntry, 12, 12);
#define CAMERA_AT(member, at32, at64) \
    _Static_assert(offsetof(SetupIntroCamera, member) == (IS_64_BIT ? (at64) : (at32)), "SetupIntroCamera." #member)
CAMERA_AT(unk18, 0x18, 0x18);
CAMERA_AT(lang1c, 0x1C, 0x20);
CAMERA_AT(lang20, 0x20, 0x28);
CAMERA_AT(prev, 0x24, 0x30);

/* VERTEX_COLLISION_NODE (bondtypes.h): no link is NULL; at 64 bits a link is
 * the base plus the 32-bit offset, at 32 bits the stored pointer. */
static int checkVertexCollisionNode(void)
{
    static u8 file[0x100];
    Vertex vertex = {0};
    int failures = 0;

#if IS_64_BIT
    vertex.LinkOffset = 0;
    failures += VERTEX_COLLISION_NODE(file, vertex) != NULL;
    vertex.LinkOffset = 0x40;
    failures += VERTEX_COLLISION_NODE(file, vertex) != (void *) &file[0x40];
#else
    vertex.CollisionRelatedNode = NULL;
    failures += VERTEX_COLLISION_NODE(file, vertex) != NULL;
    vertex.CollisionRelatedNode = &file[0x40];
    failures += VERTEX_COLLISION_NODE(file, vertex) != (void *) &file[0x40];
#endif
    if (failures != 0)
    {
        printf("ABI probe FAIL: VERTEX_COLLISION_NODE (%d cases)\n", failures);
    }
    return failures;
}

#if IS_64_BIT
/* portRenderDataFromWords (port_model_words.h) puts each 32-bit template word
 * in the member at that word's 32-bit offset; pointer words are NULL. */
static int checkRenderDataFromWords(void)
{
    static const u32 words[15] = {1, 3, 0, 0, 5, 6, 7, 8, 9, 10, 11, 12, 0x11223344, 0x55667788, 15};
    ModelRenderData rd;
    int failures = 0;

    memset(&rd, 0xff, sizeof(rd));
    portRenderDataFromWords(&rd, words);
    failures += rd.basemtx != NULL || rd.gdl != NULL || rd.mtxlist != NULL;
    failures += rd.zbufferenabled != 1 || rd.flags != 3;
    failures += rd.unk14 != 5 || rd.unk18 != 6 || rd.unk1c != 7 || rd.unk20 != 8;
    failures += rd.unk24 != 9 || rd.unk28 != 10 || rd.unk2c != 11 || rd.PropType != 12;
    failures += rd.envcolour.word != 0x11223344 || rd.fogcolour.word != 0x55667788 || rd.cullmode != 15;
    if (failures != 0)
    {
        printf("ABI probe FAIL: portRenderDataFromWords (%d cases)\n", failures);
    }
    return failures;
}
#endif

/* The commands buildGaugeBarDL (glass2.c) emits for numvertices: its loop,
 * counted instead of written. */
static int gaugeBarCommands(int numvertices)
{
    int commands = 1; /* the end */
    int i;

    for (i = 0; i <= numvertices / 2 - 2; i++)
    {
        commands++; /* the vertex load */
        commands += i >= 9 ? (i + 3) % 4 != 0 : (i & 1) == 0;
    }
    return commands;
}

/* bondview2.c builds both watch bars with 0x2E vertices, each in the run from
 * its s32 to the next named member. */
static int checkGaugeBarRooms(void)
{
    const size_t need = (size_t) gaugeBarCommands(0x2E);
    int failures = 0;

    failures += PLAYER_DL_ROOM(watch_body_armor_bar_gdl, watch_health_bar_gdl) < need;
    failures += PLAYER_DL_ROOM(watch_health_bar_gdl, buffer_for_watch_greenbackdrop_DL) < need;
    failures += !PLAYER_DL_ALIGNED(watch_body_armor_bar_gdl) || !PLAYER_DL_ALIGNED(watch_health_bar_gdl);
    if (failures != 0)
    {
        printf("ABI probe FAIL: the watch bar lists need %zu commands each (%d cases)\n", need, failures);
    }
    return failures;
}

int main(void)
{
    if (checkVertexCollisionNode() != 0 || checkGaugeBarRooms() != 0 || checkImageEntryHitSound() != 0)
    {
        return 1;
    }
#if IS_64_BIT
    if (checkRenderDataFromWords() != 0)
    {
        return 1;
    }
#endif
    printf("ABI probe OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
