/* Exercise real collision callbacks with synthetic geometry, including Wasm ABI. */
#include <stdlib.h>
#include "../../src/game/stan.c"

void ge_dbg_vec(const char *tag, const f32 *values, int count)
{
    (void)tag; (void)values; (void)count;
}
void osSyncPrintf(const char *format, ...) { (void)format; abort(); }

_Static_assert(__builtin_types_compatible_p(__typeof__(&stanCheckLinkedSpecialTile),
    standTileLocusCallback_B_t), "Special-tile callback must match the walker's float parameters");
_Static_assert(__builtin_types_compatible_p(__typeof__(&stanLocusEdgeAboveYCallback),
    standTileLocusCallback_B_t), "Threshold adapter must match the walker's context type");

/* A tile whose edge links to a ladder tile is reported through the record's
 * count, which the movement code reads with stanGetLocusCount. */
static void testLadderLinkIsCounted(void)
{
    /* Arrange: tile 16 of a synthetic tile array is a ladder (special 3) and
     * the source tile's first edge links to it. */
    _Alignas(StandTile) u8 tiles[17 * sizeof(StandTile)] = {0};
    _Alignas(StandTile) u8 source[sizeof(StandTile) + 3 * sizeof(StandTilePoint)] = {0};
    StandTile *ladder = (StandTile *)(tiles + 16 * sizeof(StandTile));
    StandTile *tile = (StandTile *)source;
    struct StandTileLocusCallbackRecord record = {0};
    ladder->mid.half = 3;
    tile->points[0].link = 16;
    standTileStart = (StandTile *)tiles;

    /* Act */
    s32 result = stanCheckLinkedSpecialTile(tile, 0, 0.0f, 0.0f, 0.0f, &record);
    standTileStart = NULL;

    /* Assert: the ladder is counted and the crouch flag stays clear. */
    if (result != 0 || stanGetLocusCount(&record) != 1 || stanGetLocusField0(&record) != 0) abort();
}

/* A tile whose edge links to a crouch tile sets the record's first field,
 * which stanGetLocusField0 returns as a flag (LP64-004 Task 13: the branch
 * that stores 1 in the record's pointer-sized field). */
static void testCrouchLinkSetsTheFlag(void)
{
    /* Arrange: tile 16 of a synthetic tile array is a crouch tile (special
     * 1) and the source tile's first edge links to it. */
    _Alignas(StandTile) u8 tiles[17 * sizeof(StandTile)] = {0};
    _Alignas(StandTile) u8 source[sizeof(StandTile) + 3 * sizeof(StandTilePoint)] = {0};
    StandTile *crouch = (StandTile *)(tiles + 16 * sizeof(StandTile));
    StandTile *tile = (StandTile *)source;
    struct StandTileLocusCallbackRecord record = {0};
    crouch->mid.half = 1;
    tile->points[0].link = 16;
    standTileStart = (StandTile *)tiles;

    /* Act */
    s32 result = stanCheckLinkedSpecialTile(tile, 0, 0.0f, 0.0f, 0.0f, &record);
    standTileStart = NULL;

    /* Assert: the crouch flag is set and read back; no ladder is counted. */
    if (result != 1 || record.rooms != (s32 *)(intptr_t)1 || stanGetLocusField0(&record) == 0
        || stanGetLocusCount(&record) != 0) abort();
}

int main(void)
{
    testLadderLinkIsCounted();
    testCrouchLinkSetsTheFlag();

    /* Arrange: a triangular tile with three solid edges, no ROM-backed links. */
    _Alignas(StandTile) u8 storage[sizeof(StandTile) + 3 * sizeof(StandTilePoint)] = {0};
    StandTile *tile = (StandTile *)storage;
    struct StandTileLocusCallbackRecord record = {0};
    tile->tail.half = 3;
    tile->points[1].x = 100;
    tile->points[2].z = 100;

    /* Act: proximity to a wall invokes callback B with three f32 arguments. */
    s32 result = stanTileDistanceRelated(&tile, 1.0f, 1.0f, 20.0f, &record);

    /* Assert: a real collision is returned without an indirect-call trap. */
    if (result != STAN_COLLISION_FOUND) abort();
    (void)stanTestLocusEdgeAboveY(&tile, 1.0f, 1.0f, 20.0f, 5.0f);
    return 0;
}
