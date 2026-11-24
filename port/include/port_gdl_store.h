/**
 * The models' converted display lists at 64 bits (LP64-006 Task 11b).
 *
 * A model's node lists are converted into blocks of the high arena
 * (portAllocHigh, pcsched.c), one block a list, and the node is pointed at its
 * block (sub_GAME_7F0762E0, objecthandler_2.c). The arena only grows, and until
 * this file every load of every model took new blocks: 10,240 KB a lap of the
 * soak, the 64 MB arena full after about six. A 32-bit build still does.
 *
 * The rule. The store remembers, for each block, the address of the node
 * record that points at it (ModelNode.Data, inside the model's file image).
 * A block is given back when a later model load writes its image over that
 * address: the record that held the pointer is then another model's bytes.
 * Nothing else gives a block back. A list takes the smallest block that was
 * given back and holds its worst case, and only once every frame handed to
 * the renderer before the give-back has been drawn (portGfxFramesFinished),
 * so a display list the renderer still holds cannot run a block that now
 * holds another list. The frame being built is not counted: the rule relies
 * on model loads happening between frame builds (the stage loads, the hand
 * room loads of gun.c), as the original game's lists, which lived in the
 * image, also did.
 * The block is zeroed first, as a new block of the arena is. A model loaded
 * anywhere else leaves the block alone, so two models that are both loaded
 * never share one. The arena then grows only when a list finds no block large
 * enough: until the store holds what the fullest moment of play needs.
 *
 * In a sanitizer build (GE_SANITIZE, port_poison.h) a block that was given
 * back is poisoned until it is taken again: a read of it through a pointer
 * that outlived the model is reported.
 *
 * The file is compiled only at 64 bits (CMakeLists.txt) and by its ROM-free
 * test at both widths (scripts/port/test_gdl_store.c).
 */
#ifndef PORT_GDL_STORE_H
#define PORT_GDL_STORE_H

#include <ultra64.h>

struct ModelNode;
struct texpool;

/**
 * A model's file image now stands at [START, START + BYTES): every block
 * whose node record lies there is given back. CALLERROOM is true for an image
 * loaded into a caller's buffer (a gun room, the wallet room), whose address
 * the same stage may load again; the lists converted for it until the next
 * call then make the part cache forget their vertices when they take a block
 * again (roc_hooks.c: its key is the list's address and the vertices').
 */
void portModelGdlImageLoaded(const void *start, u32 bytes, s32 callerRoom);

/**
 * Converts NODE's list SRC (texLoadFromGdl, with POOL) into a block of
 * (N * 4 + 96) commands for a list of N, the worst case, as the 32-bit
 * converter does. Returns the block, or NULL when SRC has no end within
 * 0x4000 commands or the arena gave none; the node then keeps SRC.
 */
Gfx *portModelGdlConvert(struct ModelNode *node, Gfx *src, struct texpool *pool);

/** The store beneath them: gives back the blocks whose holder lies in [START, START + BYTES). */
void portGdlStoreGiveBack(const void *start, u32 bytes);

/**
 * A block of at least BYTES for the record at HOLDER: the smallest one that
 * was given back and holds BYTES, with no frame from before its give-back
 * unfinished, zeroed; else a new one of BYTES from the arena. *AGAIN is set to
 * whether it had been given back. NULL when the arena gave none.
 */
void *portGdlStoreTake(const void *holder, u32 bytes, s32 *again);

/** How many blocks are held by a record and how many wait to be taken again, and the bytes of both. */
void portGdlStoreCounts(u32 *held, u32 *spare, u32 *bytes);

#endif
