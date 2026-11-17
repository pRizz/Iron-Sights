/**
 * Model read-write data and render setup at both pointer widths (LP64-003).
 *
 * A model's read-write data is an array of union ModelRwData * slots
 * (Model.datas): each node's record starts at the slot its RwDataIndex names
 * and covers as many slots as it needs. A slot is 4 bytes at 32 bits and 8 at
 * 64, so the unit count and the storage are counted in slots, not in 4-byte
 * words. At 32 bits each macro is the expression its sites had, so they
 * compile to the code they had.
 *
 *   MODEL_RWDATA_UNITS(record)  slots a record type takes (rounded up)
 *   MODEL_RWDATA_BYTES(count)   bytes of COUNT slots, as an int
 *   MODEL_NODE_SLOT_BYTES(n)    byte offset of entry N of a ModelNode * array
 *   MODEL_STORAGE_PAD           a Model's size plus 0x3f, for an ALIGN64 step
 *
 * No record takes more slots at 64 bits than at 32 (the header record is 96
 * bytes at both widths: 24 slots, then 12), so runs sized for the N64's slot
 * counts hold every model at 64 bits once each slot is pointer-sized.
 *
 * At 64 bits portRenderDataFromWords builds a ModelRenderData from the N64's
 * 16-word render template by member (gunfire.c's templates live in gun.c as
 * u32 arrays). Needs bondtypes.h first.
 */
#ifndef PORT_MODEL_WORDS_H
#define PORT_MODEL_WORDS_H

#include "platform_info.h"

#if IS_64_BIT
#define MODEL_RWDATA_UNITS(record) \
    ((sizeof(record) + sizeof(union ModelRwData *) - 1) / sizeof(union ModelRwData *))
#define MODEL_RWDATA_BYTES(count) ((count) * (s32) sizeof(union ModelRwData *))
#define MODEL_NODE_SLOT_BYTES(n) ((n) * (s32) sizeof(ModelNode *))
#define MODEL_STORAGE_PAD ((s32) sizeof(Model) + 0x3f)

/**
 * Fills RD from words 1 to 15 of a 32-bit ModelRenderData template (WORDS[0]
 * is the word at offset 4). Word 0 (basemtx), word 3 (gdl) and word 4
 * (mtxlist) are pointers, which every template holds as 0 and a 32-bit word
 * cannot be at 64 bits, so they are NULL. The 32-bit layout, word by word:
 * 0 basemtx, 1 zbufferenabled, 2 flags, 3 gdl, 4 mtxlist, 5 to 11 unk14 to
 * unk2c, 12 PropType, 13 envcolour, 14 fogcolour, 15 cullmode.
 */
static inline void portRenderDataFromWords(ModelRenderData *rd, const u32 *words)
{
    rd->basemtx = NULL;
    rd->zbufferenabled = (bool) words[0];
    rd->flags = words[1];
    rd->gdl = NULL;
    rd->mtxlist = NULL;
    rd->unk14 = words[4];
    rd->unk18 = words[5];
    rd->unk1c = words[6];
    rd->unk20 = words[7];
    rd->unk24 = words[8];
    rd->unk28 = words[9];
    rd->unk2c = words[10];
    rd->PropType = (s32) words[11];
    rd->envcolour.word = words[12];
    rd->fogcolour.word = words[13];
    rd->cullmode = words[14];
}
#else
#define MODEL_RWDATA_UNITS(record) (sizeof(record) / 4)
#define MODEL_RWDATA_BYTES(count) ((count) << 2)
#define MODEL_NODE_SLOT_BYTES(n) ((n) << 2)
#define MODEL_STORAGE_PAD 0xfb
#endif

#endif
