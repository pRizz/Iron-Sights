/* Baked Lighting's ray tracer (FORK-082) on a known scene: a floor and a wall
 * standing on it. A floor vertex at the wall's foot must darken, one far out
 * in the open must keep its colour, a vertex facing into the wall must come
 * out darkest, and the viewmodel probe must darken beside the wall and stay 1
 * in the open. Built by test_baked_trace.sh with the SDL shim. */
#include <assert.h>
#include <stdio.h>
#include "../../port/src/port_baked_trace.c"
#include "../../port/src/port_baked_sort.h"

enum { FLOOR0, FLOOR1, FLOOR2, FLOOR3, WALL0, WALL1, WALL2, WALL3, CORNER, OPEN, AWAY, NVERTS };

static float pos[NVERTS * 3] = {
    -100, 0, -100,  100, 0, -100,  100, 0, 100,  -100, 0, 100, /* floor at y = 0 */
    10, 0, -100,  10, 0, 100,  10, 100, 100,  10, 100, -100,  /* wall at x = 10, facing -x */
    9, 0, 0,     /* at the wall's foot */
    -80, 0, 0,   /* in the open */
    9, 50, 0,    /* on the wall's face... */
};
static float nrm[NVERTS * 3] = {
    0, 1, 0,  0, 1, 0,  0, 1, 0,  0, 1, 0,
    -1, 0, 0,  -1, 0, 0,  -1, 0, 0,  -1, 0, 0,
    0, 1, 0,
    0, 1, 0,
    1, 0, 0,     /* ...but facing into it: its rays leave through the wall's back */
};
static uint32_t tris[] = {
    FLOOR0, FLOOR1, FLOOR2,  FLOOR0, FLOOR2, FLOOR3,
    WALL0, WALL1, WALL2,     WALL0, WALL2, WALL3,
};

/* The stable sort a macOS build orders the tree's triangles with
 * (port_baked_sort.h): equal keys keep the order they came in, which is what
 * glibc's qsort gives the Linux builds and macOS's qsort does not. */
static void stableSortKeepsEqualKeysInOrder(void)
{
    /* Arrange: the key of triangle i is cent[i * 3 + 1]; several are equal */
    static const float cent[10 * 3] = {
        0, 9.0f, 0,  0, 0.0f, 0,  0, 0.5f, 0,  0, 0.0f, 0,  0, 9.0f, 0,
        0, 1.0f, 0,  0, 9.0f, 0,  0, 1.0f, 0,  0, 9.0f, 0,  0, 1.0f, 0,
    };
    uint32_t order[6] = { 5, 3, 9, 1, 7, 2 };
    uint32_t scratch[6];
    static const uint32_t expected[6] = { 3, 1, 2, 5, 9, 7 };

    /* Act */
    portBakeStableSort(order, 6, scratch, cent, 1);
    /* Assert */
    assert(memcmp(order, expected, sizeof(expected)) == 0);
}

static void stableSortMatchesAnInsertionSortOnManyTies(void)
{
    enum { COUNT = 1000 };
    static float cent[COUNT * 3];
    static uint32_t order[COUNT], reference[COUNT], scratch[COUNT];
    uint32_t state = 12345u;

    /* Arrange: 1,000 triangles in a shuffled order with only 7 distinct keys */
    for (uint32_t i = 0; i < COUNT; i++) {
        state = state * 1664525u + 1013904223u;
        cent[i * 3 + 2] = (float) ((state >> 16) % 7u);
        order[i] = i;
    }
    for (uint32_t i = COUNT - 1; i > 0; i--) {
        uint32_t j, swap;
        state = state * 1664525u + 1013904223u;
        j = (state >> 8) % (i + 1);
        swap = order[i]; order[i] = order[j]; order[j] = swap;
    }
    memcpy(reference, order, sizeof(order));
    for (uint32_t i = 1; i < COUNT; i++) { /* an insertion sort is stable */
        const uint32_t moved = reference[i];
        uint32_t at = i;
        while (at > 0 && cent[reference[at - 1] * 3 + 2] > cent[moved * 3 + 2]) {
            reference[at] = reference[at - 1];
            at--;
        }
        reference[at] = moved;
    }
    /* Act */
    portBakeStableSort(order, COUNT, scratch, cent, 2);
    /* Assert */
    assert(memcmp(order, reference, sizeof(order)) == 0);
}

static void stableSortLeavesAnEmptyOrSingleListAlone(void)
{
    static const float cent[3] = { 1.0f, 2.0f, 3.0f };
    uint32_t order[1] = { 0 };
    uint32_t scratch[1] = { 77 };

    /* Act */
    portBakeStableSort(order, 0, scratch, cent, 0);
    portBakeStableSort(order, 1, scratch, cent, 0);
    /* Assert */
    assert(order[0] == 0);
}

int main(void)
{
    uint8_t orig[NVERTS * 3], baked[NVERTS * 3];
    struct BakeLevel lvl = { NVERTS, pos, nrm, orig, baked, 4, tris };
    struct BakeParams p = { 20.0f, 256, 0.85f, 0.6f };
    float probe[3];
    int guard = 0;

    memset(orig, 200, sizeof(orig));
    memcpy(baked, orig, sizeof(orig));
    portBakeTraceStart(&lvl, &p);
    while (!portBakeTraceStep(1000000) && guard++ < 1000) {
    }
    assert(portBakeTraceProgress() == 1.0f);

    /* the wall's foot: a quarter to a half of the hemisphere is wall */
    assert(baked[CORNER * 3] < 185 && baked[CORNER * 3] > 90);
    /* nothing within the radius: the file colour exactly */
    assert(baked[OPEN * 3] == 200 && baked[OPEN * 3 + 1] == 200);
    /* a normal pointing into the wall just in front: nearly every ray hits it
     * at once, so it comes out darkest (why the normals must face out) */
    assert(baked[AWAY * 3] < 120 && baked[AWAY * 3] < baked[CORNER * 3]);
    /* grey surroundings tint nothing */
    assert(baked[CORNER * 3] == baked[CORNER * 3 + 1] && baked[CORNER * 3 + 1] == baked[CORNER * 3 + 2]);

    /* the probe: beside the wall, the surfaces it hits are darkened */
    portBakeTraceProbe((const float[3]) { 8.0f, 5.0f, 0.0f }, probe);
    assert(probe[0] < 1.0f && probe[0] > 0.3f);
    portBakeTraceProbe((const float[3]) { -80.0f, 50.0f, 0.0f }, probe);
    assert(probe[0] == 1.0f && probe[1] == 1.0f && probe[2] == 1.0f);

    /* the encode helpers port_baked_light.c relies on */
    {
        float lin[256];
        portBakeLinTable(lin);
        assert(lin[0] == 0.0f && lin[255] > 0.999f && lin[255] < 1.001f);
        assert(portBakeEncode(lin[128]) == 128 && portBakeEncode(2.0f) == 255 && portBakeEncode(-1.0f) == 0);
        assert(portBakeSqrt(16.0f) == 4.0f);
    }
    portBakeTraceCancel();
    stableSortKeepsEqualKeysInOrder();
    stableSortMatchesAnInsertionSortOnManyTies();
    stableSortLeavesAnEmptyOrSingleListAlone();
    printf("Baked Lighting trace tests passed (corner %u, open %u, away %u).\n", baked[CORNER * 3],
           baked[OPEN * 3], baked[AWAY * 3]);
    return 0;
}
