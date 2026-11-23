/* LP64-006 Task 11b: the models' converted display lists (port_gdl_store.h).
 * A block of the high arena is given back when a later model load writes its
 * image over the node record that pointed at the block, and is taken again
 * by a list it holds once no frame from before is unfinished. The models, the
 * arena, the frame counts and the converter are invented here,
 * no ROM. test_buffers.sh builds this at both widths, plain and sanitized;
 * the game compiles the store only at 64 bits. */
#include "port/src/port_gdl_store.c"
#include "test_buffer_helpers.h"
#include <string.h>

#if defined(__SANITIZE_ADDRESS__)
int __asan_address_is_poisoned(void const volatile *address);
#endif

/* ---- what the store reaches outside itself ---------------------------- */

static u8 s_Arena[0x40000] __attribute__((aligned(16)));
static u32 s_ArenaUsed;
static u32 s_Submitted, s_Finished;
static const void *s_Forgotten[8];
static s32 s_ForgottenCount;
static s32 s_ConverterAddsOne; /* the converter writes one command more than it read, as a G_SETTEX expands */

void *portAllocHigh(u32 size)
{
    void *block = s_Arena + s_ArenaUsed;

    size = (size + 15u) & ~15u;
    assert(s_ArenaUsed + size <= sizeof(s_Arena));
    s_ArenaUsed += size;
    return block;
}

void portHighArenaReport(void) {}
u32 portGfxFramesSubmitted(void) { return s_Submitted; }
u32 portGfxFramesFinished(void) { return s_Finished; }

void portPartCacheVerticesFreed(const void *vtx)
{
    assert(s_ForgottenCount < 8);
    s_Forgotten[s_ForgottenCount++] = vtx;
}

s32 texLoadFromGdl(Gfx *src, s32 srcsize, Gfx *dst, void *texpool)
{
    s32 commands = srcsize / (s32) sizeof(Gfx);

    (void) texpool;
    memcpy(dst, src, (size_t) srcsize);
    if (s_ConverterAddsOne)
    {
        dst[commands].words.w0 = 0x12345678u;
        commands++;
    }
    return commands * (s32) sizeof(Gfx);
}

/* ---- invented models --------------------------------------------------- */

#define END_COMMAND ((uintptr_t) (u8) G_ENDDL << 24)

/* A model's file image: two nodes and their records. */
typedef struct TestImage
{
    ModelNode node[2];
    union ModelRoData record[2];
} TestImage;

static TestImage s_Room[2]; /* two buffers a caller loads models into */

static u32 blockBytes(s32 commands) { return (u32) (commands * 4 + 96) * (u32) sizeof(Gfx); }

/* A list of COMMANDS commands, the last its end, the others carrying TAG. */
static void makeList(Gfx *list, s32 commands, u32 tag)
{
    s32 i;

    for (i = 0; i < commands - 1; i++)
    {
        list[i].words.w0 = ((uintptr_t) (u8) G_RDPPIPESYNC << 24) | tag;
        list[i].words.w1 = (uintptr_t) i;
    }
    list[commands - 1].words.w0 = END_COMMAND;
    list[commands - 1].words.w1 = 0;
}

/* The loader's half: the image is written at IMAGE, then the store is told. */
static void loadImage(TestImage *image, s32 callerRoom)
{
    memset(image, 0, sizeof(*image));
    portModelGdlImageLoaded(image, (u32) sizeof(*image), callerRoom);
}

static Gfx *convertNode(TestImage *image, s32 index, u16 opcode, Gfx *list)
{
    image->node[index].Opcode = opcode;
    image->node[index].Data = &image->record[index];
    return portModelGdlConvert(&image->node[index], list, NULL);
}

/* Each test starts with an empty store and an empty arena. */
static void startTest(void)
{
    free(s_Blocks);
    s_Blocks = NULL;
    s_BlockCount = s_BlockRoom = 0;
    s_SpareFirst = -1;
    s_Held = s_Spare = s_Bytes = 0;
    PORT_UNPOISON(s_Arena, sizeof(s_Arena));
    s_ArenaUsed = 0;
    s_Submitted = s_Finished = 0;
    s_ForgottenCount = 0;
    s_ConverterAddsOne = 0;
}

/* ---- the tests --------------------------------------------------------- */

static void testASecondLoadOfTheSameModelTakesNoArenaBytes(void)
{
    /* Arrange: a model of two lists, loaded once */
    Gfx first[5], second[9];
    Gfx *blocks[2], *again[2];
    u32 usedAfterFirstLoad;

    startTest();
    makeList(first, 5, 1);
    makeList(second, 9, 2);
    loadImage(&s_Room[0], 1);
    blocks[0] = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, first);
    blocks[1] = convertNode(&s_Room[0], 1, MODELNODE_OPCODE_DL, second);
    usedAfterFirstLoad = s_ArenaUsed;

    /* Act: the same model is loaded at the same address */
    loadImage(&s_Room[0], 1);
    again[0] = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, first);
    again[1] = convertNode(&s_Room[0], 1, MODELNODE_OPCODE_DL, second);

    /* Assert: no new bytes, each node has its own block again, converted anew */
    assert(blocks[0] != NULL && blocks[1] != NULL && blocks[0] != blocks[1]);
    assert(s_ArenaUsed == usedAfterFirstLoad);
    assert(again[0] == blocks[0] && again[1] == blocks[1]);
    assert(memcmp(again[0], first, sizeof(first)) == 0);
    assert(memcmp(again[1], second, sizeof(second)) == 0);
}

static void testABlockGivenBackIsTakenByAnotherModelsListOfTheSameSize(void)
{
    /* Arrange: a model whose converter wrote one command more than the list held */
    Gfx old[6], new[6];
    Gfx *oldBlock, *newBlock;
    u32 usedBefore;

    startTest();
    makeList(old, 6, 3);
    makeList(new, 6, 4);
    loadImage(&s_Room[0], 1);
    s_ConverterAddsOne = 1;
    oldBlock = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, old);
    s_ConverterAddsOne = 0;
    usedBefore = s_ArenaUsed;

    /* Act: another model is loaded over it; its list hangs from the other node */
    loadImage(&s_Room[0], 1);
    newBlock = convertNode(&s_Room[0], 1, MODELNODE_OPCODE_DL, new);

    /* Assert: the block is reused, holds the new list, and nothing of the old one after it */
    assert(newBlock == oldBlock);
    assert(s_ArenaUsed == usedBefore);
    assert(memcmp(newBlock, new, sizeof(new)) == 0);
    assert(newBlock[6].words.w0 == 0 && newBlock[6].words.w1 == 0);
}

static void testAModelLoadedElsewhereGivesNothingBack(void)
{
    /* Arrange: a model loaded in one room */
    Gfx list[7];
    Gfx *here, *there;
    u32 usedBefore;

    startTest();
    makeList(list, 7, 5);
    loadImage(&s_Room[0], 1);
    here = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, list);
    usedBefore = s_ArenaUsed;

    /* Act: the same model is loaded in the other room; both now stand */
    loadImage(&s_Room[1], 1);
    there = convertNode(&s_Room[1], 0, MODELNODE_OPCODE_DL, list);

    /* Assert: it took a block of its own, and the first model's is whole */
    assert(there != here);
    assert(s_ArenaUsed == usedBefore + blockBytes(7));
    assert(memcmp(here, list, sizeof(list)) == 0);
}

static void testALongerListTakesANewBlock(void)
{
    /* Arrange: a list of 10 commands, loaded and then written over */
    Gfx ten[10], eleven[11];
    Gfx *tenBlock, *elevenBlock;
    u32 usedBefore;

    startTest();
    makeList(ten, 10, 6);
    makeList(eleven, 11, 7);
    loadImage(&s_Room[0], 1);
    tenBlock = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, ten);
    usedBefore = s_ArenaUsed;

    /* Act: the model that replaces it has a list of 11 */
    loadImage(&s_Room[0], 1);
    elevenBlock = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, eleven);

    /* Assert: the block of 10 is not stretched to hold it */
    assert(elevenBlock != tenBlock);
    assert(s_ArenaUsed == usedBefore + blockBytes(11));
}

static void testAShorterListTakesTheSmallestBlockThatHoldsIt(void)
{
    /* Arrange: a model with lists of 30 and 20 commands, loaded and then written over by an empty model */
    Gfx thirty[30], twenty[20], eighteen[18];
    Gfx *thirtyBlock, *twentyBlock, *eighteenBlock;
    u32 usedBefore;

    startTest();
    makeList(thirty, 30, 13);
    makeList(twenty, 20, 14);
    makeList(eighteen, 18, 15);
    loadImage(&s_Room[0], 1);
    thirtyBlock = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, thirty);
    twentyBlock = convertNode(&s_Room[0], 1, MODELNODE_OPCODE_DL, twenty);
    loadImage(&s_Room[0], 1);
    usedBefore = s_ArenaUsed;

    /* Act: a list of 18, which either block would hold */
    eighteenBlock = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, eighteen);

    /* Assert: it took the block of 20 and left the block of 30 for a longer list */
    assert(thirtyBlock != twentyBlock);
    assert(eighteenBlock == twentyBlock);
    assert(s_ArenaUsed == usedBefore);
    assert(memcmp(eighteenBlock, eighteen, sizeof(eighteen)) == 0);
}

static void testOnlyRecordsInsideTheImageGiveTheirBlocksBack(void)
{
    /* Arrange: three records in a row, each holding a block of its own size */
    static u8 memory[48];
    s32 again;
    u32 heldBelow, heldInside, heldAfter, spare, bytes;

    startTest();
    portGdlStoreTake(memory + 15, 0x100, &again);
    portGdlStoreTake(memory + 16, 0x200, &again);
    portGdlStoreTake(memory + 32, 0x300, &again);

    /* Act: an image of 16 bytes is loaded at the middle record, then one a byte longer */
    portGdlStoreGiveBack(memory + 16, 16);
    portGdlStoreCounts(&heldInside, &spare, &bytes);
    portGdlStoreGiveBack(memory + 16, 17);
    portGdlStoreCounts(&heldAfter, &spare, &bytes);
    portGdlStoreGiveBack(memory + 16, 0);
    portGdlStoreCounts(&heldBelow, &spare, &bytes);

    /* Assert: the record below the image and the one at its end keep their blocks */
    assert(heldInside == 2);
    assert(heldAfter == 1 && heldBelow == 1);
    assert(spare == 2);
}

static void testABlockWaitsForTheFramesBuiltBeforeItWasGivenBack(void)
{
    /* Arrange: a model drawn by frame 5, which the renderer has not finished */
    Gfx list[12];
    Gfx *drawn, *early, *late;
    u32 usedBefore;

    startTest();
    makeList(list, 12, 8);
    loadImage(&s_Room[0], 1);
    drawn = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, list);
    s_Submitted = 5;
    s_Finished = 4;
    usedBefore = s_ArenaUsed;

    /* Act: the model is replaced while frame 5 runs, and again once it is done */
    loadImage(&s_Room[0], 1);
    early = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, list);
    s_Finished = 5;
    loadImage(&s_Room[0], 1);
    late = convertNode(&s_Room[0], 1, MODELNODE_OPCODE_DL, list);

    /* Assert: frame 5's block was left alone until frame 5 was done */
    assert(early != drawn);
    assert(s_ArenaUsed == usedBefore + blockBytes(12));
    assert(late == drawn || late == early);
}

static void testAnUnendedListTakesNothing(void)
{
    /* Arrange: 0x4002 commands with no end among them */
    static Gfx endless[0x4002];
    u32 usedBefore;
    Gfx *block;
    s32 i;

    startTest();
    for (i = 0; i < 0x4002; i++)
    {
        endless[i].words.w0 = (uintptr_t) (u8) G_RDPPIPESYNC << 24;
    }
    loadImage(&s_Room[0], 1);
    usedBefore = s_ArenaUsed;

    /* Act */
    block = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, endless);

    /* Assert */
    assert(block == NULL);
    assert(s_ArenaUsed == usedBefore);
}

static void testARoomsListForgetsItsCachedPartWhenItTakesABlockAgain(void)
{
    /* Arrange: a gun-list node and a part node, loaded once into a room: new blocks, nothing cached under them */
    Gfx list[13];
    u8 base[16], vertices[16];
    s32 forgottenAtFirstLoad;

    startTest();
    makeList(list, 13, 9);
    loadImage(&s_Room[0], 1);
    convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, list);
    convertNode(&s_Room[0], 1, MODELNODE_OPCODE_DLCOLLISION, list);
    forgottenAtFirstLoad = s_ForgottenCount;

    /* Act: the room is loaded again, each record naming its vertices */
    loadImage(&s_Room[0], 1);
    s_Room[0].record[0].DisplayList.BaseAddr = base;
    s_Room[0].record[1].DisplayListCollisions.Vertices = (Vertex *) vertices;
    convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, list);
    convertNode(&s_Room[0], 1, MODELNODE_OPCODE_DLCOLLISION, list);

    /* Assert: the cache was told of the vertex base each list is drawn with */
    assert(forgottenAtFirstLoad == 0);
    assert(s_ForgottenCount == 2);
    assert(s_Forgotten[0] == base && s_Forgotten[1] == vertices);
}

static void testABankModelsListLeavesThePartCacheAlone(void)
{
    /* Arrange: a model in the stage bank, whose addresses one stage never loads twice */
    Gfx list[14];
    u8 vertices[16];

    startTest();
    makeList(list, 14, 10);
    loadImage(&s_Room[1], 0);
    convertNode(&s_Room[1], 0, MODELNODE_OPCODE_DLCOLLISION, list);

    /* Act: the next stage loads a model there (the cache is emptied at each stage load) */
    loadImage(&s_Room[1], 0);
    s_Room[1].record[0].DisplayListCollisions.Vertices = (Vertex *) vertices;
    convertNode(&s_Room[1], 0, MODELNODE_OPCODE_DLCOLLISION, list);

    /* Assert */
    assert(s_ForgottenCount == 0);
}

static void testTheCountsFollowTheBlocks(void)
{
    /* Arrange: one model of one list */
    Gfx list[15];
    u32 held, spare, bytes, heldAfter, spareAfter, bytesAfter;

    startTest();
    makeList(list, 15, 11);
    loadImage(&s_Room[0], 1);
    convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, list);
    portGdlStoreCounts(&held, &spare, &bytes);

    /* Act: the model is written over */
    loadImage(&s_Room[0], 1);
    portGdlStoreCounts(&heldAfter, &spareAfter, &bytesAfter);

    /* Assert: the block went from held to spare, and its bytes are the arena's */
    assert(held == 1 && spare == 0 && bytes == blockBytes(15));
    assert(heldAfter == 0 && spareAfter == 1);
    assert(bytesAfter == bytes && bytesAfter == s_ArenaUsed);
}

#if defined(__SANITIZE_ADDRESS__)
static void testABlockGivenBackIsPoisonedUntilItIsTakenAgain(void)
{
    /* Arrange: a model of one list */
    Gfx list[16];
    Gfx *block, *again;
    s32 poisonedWhileHeld, poisonedOnceGivenBack;

    startTest();
    makeList(list, 16, 12);
    loadImage(&s_Room[0], 1);
    block = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, list);
    poisonedWhileHeld = __asan_address_is_poisoned(block);

    /* Act: the model is written over, then a list takes the block */
    loadImage(&s_Room[0], 1);
    poisonedOnceGivenBack = __asan_address_is_poisoned(block)
        && __asan_address_is_poisoned((u8 *) block + blockBytes(16) - 1);
    again = convertNode(&s_Room[0], 0, MODELNODE_OPCODE_DL, list);

    /* Assert */
    assert(!poisonedWhileHeld);
    assert(poisonedOnceGivenBack);
    assert(again == block);
    assert(!__asan_address_is_poisoned(block) && !__asan_address_is_poisoned((u8 *) block + blockBytes(16) - 1));
}
#endif

int main(void)
{
    testASecondLoadOfTheSameModelTakesNoArenaBytes();
    testABlockGivenBackIsTakenByAnotherModelsListOfTheSameSize();
    testAModelLoadedElsewhereGivesNothingBack();
    testALongerListTakesANewBlock();
    testAShorterListTakesTheSmallestBlockThatHoldsIt();
    testOnlyRecordsInsideTheImageGiveTheirBlocksBack();
    testABlockWaitsForTheFramesBuiltBeforeItWasGivenBack();
    testAnUnendedListTakesNothing();
    testARoomsListForgetsItsCachedPartWhenItTakesABlockAgain();
    testABankModelsListLeavesThePartCacheAlone();
    testTheCountsFollowTheBlocks();
#if defined(__SANITIZE_ADDRESS__)
    testABlockGivenBackIsPoisonedUntilItIsTakenAgain();
#endif
    puts("Converted display list store checks passed");
    return 0;
}
