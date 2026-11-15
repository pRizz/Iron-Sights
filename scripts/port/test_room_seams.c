/* Room Seams (FORK-086): the pure helpers behind the seam backing copy
 * (port/fast3d/gfx_seam_grow.h). The grow moves only the named edges, in the
 * triangle's plane, by the grow; edge keys match in either order; the near
 * clip keeps the in-front part with interpolated attributes; the depth push
 * moves a point back along the view axis without touching x, y or w. */
#include <math.h>
#include <stdio.h>
#include "gfx_seam_grow.h"

static int failures;

static void check(int ok, const char *what)
{
    if (!ok)
    {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

static int near(float a, float b, float tol)
{
    return fabsf(a - b) <= tol;
}

/* distance from point q to the line through a and b, all in one plane */
static float line_distance3(const float a[3], const float b[3], const float q[3])
{
    const float e[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    const float d[3] = { q[0] - a[0], q[1] - a[1], q[2] - a[2] };
    const float c[3] = { e[1] * d[2] - e[2] * d[1], e[2] * d[0] - e[0] * d[2], e[0] * d[1] - e[1] * d[0] };
    return sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]) / sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
}

static void test_grow(void)
{
    /* an equilateral triangle (side 10) in the tilted plane y = z: no corner
     * is sharp enough for the cap */
    const float p[3][3] = { { 0, 0, 0 }, { 10, 0, 0 }, { 5, 6.1237244f, 6.1237244f } };
    float out[3][3];
    int i, e;

    /* every edge: each moved edge lies `grow` outside the original line,
     * still in the plane */
    check(gfx_seam_grow_plane(p, 0.5f, 7u, out), "all-edge grow succeeds");
    for (e = 0; e < 3; e++)
    {
        const int a = e, b = (e + 1) % 3;
        check(near(line_distance3(p[a], p[b], out[a]), 0.5f, 1e-4f) &&
                  near(line_distance3(p[a], p[b], out[b]), 0.5f, 1e-4f),
              "every moved edge sits the grow outside its line");
    }
    for (i = 0; i < 3; i++)
    {
        /* the plane through the corners is y = z */
        check(near(out[i][1], out[i][2], 1e-4f), "grown corners stay in the triangle's plane");
    }

    /* edge 0 only (corner 0 to corner 1): corner 2 stays, corners 0 and 1
     * slide along their other edges, and edge 0 moves by the grow */
    check(gfx_seam_grow_plane(p, 0.5f, 1u, out), "one-edge grow succeeds");
    check(near(out[2][0], p[2][0], 1e-5f) && near(out[2][1], p[2][1], 1e-5f) && near(out[2][2], p[2][2], 1e-5f),
          "the corner opposite the only moved edge stays");
    check(near(line_distance3(p[0], p[1], out[0]), 0.5f, 1e-4f) &&
              near(line_distance3(p[0], p[1], out[1]), 0.5f, 1e-4f),
          "the moved edge sits the grow outside its line");
    check(line_distance3(p[1], p[2], out[1]) < 1e-4f, "corner 1 stays on its unmoved edge");
    check(line_distance3(p[2], p[0], out[0]) < 1e-4f, "corner 0 stays on its unmoved edge");

    /* no edge, no area, no grow: corners unchanged */
    check(!gfx_seam_grow_plane(p, 0.5f, 0u, out), "no edge declines");
    {
        const float flat[3][3] = { { 0, 0, 0 }, { 5, 0, 0 }, { 10, 0, 0 } };
        check(!gfx_seam_grow_plane(flat, 0.5f, 7u, out) && out[1][0] == 5.0f, "degenerate triangle declines");
    }
    check(!gfx_seam_grow_plane(p, 0.0f, 7u, out), "zero grow declines");

    /* a needle's sharp corner moves at most three times the grow */
    {
        const float needle[3][3] = { { 0, 0, 0 }, { 400, 0, 0 }, { 400, 1, 0 } };
        gfx_seam_grow_plane(needle, 0.5f, 7u, out);
        for (i = 0; i < 3; i++)
        {
            const float d[3] = { out[i][0] - needle[i][0], out[i][1] - needle[i][1], out[i][2] - needle[i][2] };
            check(sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) <= 1.5f + 1e-4f, "needle corner is capped");
        }
    }
}

static void test_edge_key(void)
{
    const int16_t a[3] = { 100, -20, 7 }, b[3] = { -3, 400, 7 }, c[3] = { 100, -20, 8 };
    check(gfx_seam_edge_key(a, b) == gfx_seam_edge_key(b, a), "an edge has one key in either order");
    check(gfx_seam_edge_key(a, b) != gfx_seam_edge_key(c, b), "a different corner gives a different key");
    check(gfx_seam_edge_key(a, b) != gfx_seam_edge_key(a, c), "a different edge gives a different key");
}

static void test_near_clip(void)
{
    /* x, y, z, w, then one attribute; corner 2 is behind the near plane */
    const float tri[3 * 5] = { 0, 0, 0, 4, 1, 4, 0, 0, 4, 2, 0, 4, 0, -2, 7 };
    float out[4 * 5];
    int n, i;
    n = gfx_seam_clip_near(tri, 5, 1.0f, out);
    check(n == 4, "one corner behind leaves a quad");
    for (i = 0; i < n; i++)
    {
        check(out[i * 5 + 3] >= 1.0f - 1e-6f, "every clipped corner is in front");
    }
    /* corner 1 -> 2 crosses w = 1 at t = 0.5: halfway in x, y and the attribute */
    check(near(out[2 * 5 + 0], 2.0f, 1e-5f) && near(out[2 * 5 + 1], 2.0f, 1e-5f) && near(out[2 * 5 + 4], 4.5f, 1e-5f),
          "a clipped corner interpolates every float");
    {
        const float behind[3 * 5] = { 0, 0, 0, -1, 0, 1, 0, 0, -2, 0, 0, 1, 0, -1, 0 };
        check(gfx_seam_clip_near(behind, 5, 1.0f, out) == 0, "a triangle behind the eye clips away");
    }
    {
        const float front[3 * 5] = { 0, 0, 0, 4, 1, 4, 0, 0, 4, 2, 0, 4, 0, 2, 7 };
        check(gfx_seam_clip_near(front, 5, 1.0f, out) == 3 && out[2 * 5 + 4] == 7.0f, "a triangle in front stays whole");
    }
}

static void test_depth(void)
{
    /* a GL perspective (row vectors), camera looking down -z, near 2, far 100,
     * the whole matrix scaled so one world unit is 0.5 clip w (GoldenEye folds
     * the level scale into its projection the same way) */
    const float n = 2.0f, f = 100.0f, s = 0.5f;
    const float P[4][4] = { { s, 0, 0, 0 },
                            { 0, s, 0, 0 },
                            { 0, 0, -s * (f + n) / (f - n), -s },
                            { 0, 0, -s * 2.0f * f * n / (f - n), 0 } };
    const float alpha = gfx_seam_depth_ratio(P);
    float z10, w10, z14;
    check(near(gfx_seam_w_scale(P), 0.5f, 1e-6f), "w grows 0.5 per world unit");
    check(near(gfx_seam_near_w(P, alpha), n * s, 1e-4f), "the near plane's w is near times the scale");
    /* a point 10 units away, pushed 4 units back, has the depth of one 14 units away */
    w10 = 10.0f * s;
    z10 = (-10.0f) * P[2][2] + P[3][2];
    z14 = (-14.0f) * P[2][2] + P[3][2];
    check(near(gfx_seam_push_depth(z10, w10, alpha, 4.0f * s) / w10, z14 / (14.0f * s), 1e-5f),
          "the pushed depth is the depth of the point that far back");
    check(gfx_seam_push_depth(z10, -1.0f, alpha, 4.0f * s) == z10, "a corner behind the eye keeps its depth");
}

int main(void)
{
    test_grow();
    test_edge_key();
    test_near_clip();
    test_depth();
    if (failures == 0)
    {
        printf("room seams: all checks passed\n");
    }
    return failures != 0;
}
