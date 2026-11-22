/* The maths sweep (LP64-006 Task 4): for each of the six single-precision
 * functions the game calls (sinf, cosf, tanf, atanf, expf, powf), one line
 * `NAME HASH`, HASH the FNV-1a hash of the bits of 1,048,576 results, each
 * four bytes from the lowest; a NaN counts as 0x7fc00000, since the sign a
 * host gives a NaN is not what is compared. Two hosts whose lines are equal
 * computed every one of those results alike.
 *
 * Linked with port/src/port_compare_math.c (or a comparison tree's object of
 * it), tanf and atanf are the comparison build's; linked without, they are
 * the C library's. test_lp64_compare.sh builds and runs it at each width;
 * FORK-089 records the hashes of the i686, x86_64 and aarch64 containers.
 * Build with -fno-builtin, so no call is folded at compile time. The inputs
 * come from integer arithmetic and single float operations, which every host
 * rounds alike. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

float sinf(float);
float cosf(float);
float tanf(float);
float atanf(float);
float expf(float);
float powf(float, float);

#define SWEEP_RESULTS (1u << 20)
#define FNV_OFFSET 2166136261u
#define FNV_PRIME 16777619u

static uint32_t hashResult(uint32_t hash, float result)
{
    uint32_t bits;
    int i;

    memcpy(&bits, &result, sizeof(bits));
    if (result != result)
    {
        bits = 0x7fc00000u;
    }
    for (i = 0; i < 4; i++)
    {
        hash ^= (bits >> (8 * i)) & 0xffu;
        hash *= FNV_PRIME;
    }
    return hash;
}

/* The i-th angle: even i walk [-128, 128) in steps of 1/4096; odd i are any
 * finite float, from a fixed integer scramble of i. */
static float angleInput(uint32_t i)
{
    uint32_t bits;
    float value;

    if ((i & 1u) == 0)
    {
        return (float) ((int32_t) i - (int32_t) (SWEEP_RESULTS / 2)) / 4096.0f;
    }
    bits = i * 2654435761u;
    bits ^= bits >> 15;
    bits *= 2246822519u;
    if ((bits & 0x7f800000u) == 0x7f800000u)
    {
        bits &= ~0x00800000u;
    }
    memcpy(&value, &bits, sizeof(value));
    return value;
}

int main(void)
{
    uint32_t hashes[6] = {FNV_OFFSET, FNV_OFFSET, FNV_OFFSET, FNV_OFFSET, FNV_OFFSET, FNV_OFFSET};
    static const char *const names[6] = {"sinf", "cosf", "tanf", "atanf", "expf", "powf"};
    uint32_t i;
    int f;

    for (i = 0; i < SWEEP_RESULTS; i++)
    {
        float angle = angleInput(i);
        /* expf over [-104, 104): past both its overflow and its underflow */
        float exponent = (float) ((int32_t) i - (int32_t) (SWEEP_RESULTS / 2)) / 5041.0f;
        /* powf: a base in [0, 16) and an exponent in [-8, 8) */
        float base = (float) i / 65536.0f;
        float power = (float) ((int32_t) ((i * 7919u) & 4095u) - 2048) / 256.0f;

        hashes[0] = hashResult(hashes[0], sinf(angle));
        hashes[1] = hashResult(hashes[1], cosf(angle));
        hashes[2] = hashResult(hashes[2], tanf(angle));
        hashes[3] = hashResult(hashes[3], atanf(angle));
        hashes[4] = hashResult(hashes[4], expf(exponent));
        hashes[5] = hashResult(hashes[5], powf(base, power));
    }
    for (f = 0; f < 6; f++)
    {
        printf("%s %08x\n", names[f], hashes[f]);
    }
    return 0;
}
