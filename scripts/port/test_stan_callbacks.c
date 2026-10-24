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

int main(void)
{
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
