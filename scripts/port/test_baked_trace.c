/* Baked Lighting's ray tracer (FORK-082) on a known scene: a floor and a wall
 * standing on it. A floor vertex at the wall's foot must darken, one far out
 * in the open must keep its colour, a vertex facing into the wall must come
 * out darkest, and the viewmodel probe must darken beside the wall and stay 1
 * in the open. Built by test_baked_trace.sh with the SDL shim. */
#include <assert.h>
#include <stdio.h>
#include "../../port/src/port_baked_trace.c"

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
    printf("Baked Lighting trace tests passed (corner %u, open %u, away %u).\n", baked[CORNER * 3],
           baked[OPEN * 3], baked[AWAY * 3]);
    return 0;
}
