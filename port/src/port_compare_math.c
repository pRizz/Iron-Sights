/**
 * The comparison build's maths (LP64-006 Task 4). Compiled only with
 * -DGE_COMPARE=ON (CMakeLists.txt), never in a shipped build.
 *
 * The C libraries of the i686, x86_64 and aarch64 hosts do not round tanf and
 * atanf alike, so a stream compared across them would part wherever the game
 * calls one. This file defines both for a comparison build, replacing libm's:
 * each computes in double with only + - * / and comparisons, in an order C
 * fixes, and rounds once to float, so every host that rounds IEEE double
 * arithmetic to nearest gets the same bits. sinf, expf and powf stay libm's
 * (decision 2 of the LP64-006 plan): the sweep (lp64_libm_sweep.c) found the
 * three hosts' results equal. cosf joined the pair by the plan's rule for
 * the sweep: i686's glibc cosf is one ulp from the 64-bit hosts' at
 * +-0x1.0e1c8p+6 (about 67.53), two of the sweep's 1,048,576 inputs
 * (FORK-089).
 *
 * What keeps the arithmetic alike: no excess precision (the 32-bit build does
 * its double arithmetic in SSE2 registers, -msse2 -mfpmath=sse, which the
 * check below enforces, rather than x87's 80 bits), no contraction into fused
 * multiply-adds (CMake compiles this file with -ffp-contract=off at either
 * width; aarch64 would fuse without it), no -ffast-math reassociation (checked
 * below), and no call into libm. Integer arithmetic, exact on every host,
 * does the large-argument reduction.
 *
 * With GE_COMPARE_LIBM (the twin that shows what the C library changes) only
 * portCompareLogBuild is compiled and the game keeps libm's functions.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> /* atexit */
#include <string.h>

#if defined(__i386__) && !defined(__SSE2_MATH__)
#error "port_compare_math.c needs SSE2 double arithmetic (-msse2 -mfpmath=sse): x87 rounds to 80 bits"
#endif
#if defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ != 0
#error "port_compare_math.c needs FLT_EVAL_METHOD 0: every double operation rounded to double"
#endif
#ifdef __FAST_MATH__
#error "port_compare_math.c must not be built with -ffast-math: it reorders the arithmetic"
#endif

static unsigned long s_SynthDropped;

/** synthesizer.c's __allocParam: an update found the parameter pool empty and was dropped. */
void portCompareSynthDropped(void)
{
    s_SynthDropped++;
}

/* At exit: how many synthesizer updates the run dropped (LP64-006 final review, I1: the pool keeps
 * the 32-bit size at 64 bits, and this measures what that size drops at either width). */
static void compareSynthReport(void)
{
    printf("ge: compare: synthesizer updates dropped %lu\n", s_SynthDropped);
    fflush(stdout);
}

/** main.c: prints `ge: compare: on` once at boot, `on (libm)` in the twin, and arms the exit line. */
void portCompareLogBuild(void)
{
#ifdef GE_COMPARE_LIBM
    printf("ge: compare: on (libm)\n");
#else
    printf("ge: compare: on\n");
#endif
    fflush(stdout);
    atexit(compareSynthReport);
}

#ifndef GE_COMPARE_LIBM

float tanf(float x);
float cosf(float x);
float atanf(float x);

#define COMPARE_PI_2 1.5707963267948966      /* pi/2, rounded to double */
#define COMPARE_PI_4 0.7853981633974483      /* pi/4 */
#define COMPARE_ATAN_HALF 0.4636476090008061 /* atan(1/2), rounded to double */
#define COMPARE_ATAN_3_2 0.982793723247329   /* atan(3/2) */
/* pi/2 * 2^-94: one unit of the reduction's 96-bit quarter-turn fraction */
#define COMPARE_PI_2_UNIT 0x1.921fb54442d18p-94
#define COMPARE_TWO_32 4294967296.0
/* The largest float below pi/4: arguments up to it need no reduction. */
#define COMPARE_FLOAT_PI_4_BITS 0x3f490fdau
#define COMPARE_ATAN_TERMS 20

/* The first 256 bits of 2/pi after the point (0.a2f9836e...), computed with
 * exact integer arithmetic (Machin's formula). Bit i (from 1) is bit
 * 31 - (i - 1) % 32 of word (i - 1) / 32; a float needs at most bit 199. */
static const uint32_t kTwoOverPi[8] = {
    0xa2f9836eu, 0x4e441529u, 0xfc2757d1u, 0xf534ddc0u,
    0xdb629599u, 0x3c439041u, 0xfe5163abu, 0xdebbc561u,
};

static uint32_t floatBits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float bitsFloat(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Word `index` of 2/pi's bits; the bits before the point are zero. */
static uint32_t twoOverPiWord(int32_t index)
{
    return index >= 0 && index < 8 ? kTwoOverPi[index] : 0u;
}

/* The 32 bits of 2/pi from bit `first` (from 1; zero or below reads zeros). */
static uint32_t twoOverPiBits(int32_t first)
{
    int32_t offset = first - 1;
    int32_t word = offset >= 0 ? offset / 32 : -((31 - offset) / 32);
    int32_t shift = offset - word * 32;

    if (shift == 0)
    {
        return twoOverPiWord(word);
    }
    return (twoOverPiWord(word) << shift) | (twoOverPiWord(word + 1) >> (32 - shift));
}

/**
 * |x| reduced by the nearest multiple n of pi/2, for a finite x of at least
 * pi/4: returns |x| - n * pi/2 in [-pi/4, pi/4] and sets *quarter to n mod 4.
 * |x| is m * 2^e with m the 24-bit significand; its product with 2/pi, kept
 * modulo 4 (whole bits of 2/pi worth 4 or more times m drop out), is m times
 * 96 bits of 2/pi from bit e - 1, kept to 96 bits: a fixed-point fraction
 * with two whole bits. The bits past them change it by less than 2^-70.
 */
static double reduceQuarterTurns(uint32_t magnitudeBits, uint32_t *quarter)
{
    uint32_t m = (magnitudeBits & 0x007fffffu) | 0x00800000u;
    int32_t e = (int32_t) (magnitudeBits >> 23) - 150;
    uint64_t low = (uint64_t) m * twoOverPiBits(e + 63);
    uint64_t middle = (uint64_t) m * twoOverPiBits(e + 31) + (low >> 32);
    uint64_t high = (uint64_t) m * twoOverPiBits(e - 1) + (middle >> 32);
    uint32_t w2 = (uint32_t) high + (1u << 29); /* + one half, to round */
    /* The fraction less one half, in [-2^29, 2^29) units of 2^-30 quarter
     * turns: with the lower words below it, the signed remainder in
     * [-1/2, 1/2) quarter turns.
     * Each conversion is exact and each sum rounds once; a sum can cancel
     * only where its operands are exact, so the result keeps 52 bits. */
    int32_t top = (int32_t) (w2 & 0x3fffffffu) - (1 << 29);
    double fraction = ((double) top * COMPARE_TWO_32 + (double) (uint32_t) middle) * COMPARE_TWO_32 +
                      (double) (uint32_t) low;

    *quarter = w2 >> 30;
    return fraction * COMPARE_PI_2_UNIT;
}

/* sin(y) and cos(y) for |y| <= pi/4, from their Taylor series, nested
 * (z = y^2 <= 0.62: the first omitted terms are below 2^-60 of the result). */
static double sinSeries(double y)
{
    double z = y * y;

    return y * (1.0 - z / 6.0 * (1.0 - z / 20.0 * (1.0 - z / 42.0 * (1.0 - z / 72.0 *
           (1.0 - z / 110.0 * (1.0 - z / 156.0 * (1.0 - z / 210.0 * (1.0 - z / 272.0))))))));
}

static double cosSeries(double y)
{
    double z = y * y;

    return 1.0 - z / 2.0 * (1.0 - z / 12.0 * (1.0 - z / 30.0 * (1.0 - z / 56.0 *
           (1.0 - z / 90.0 * (1.0 - z / 132.0 * (1.0 - z / 182.0 * (1.0 - z / 240.0 *
           (1.0 - z / 306.0))))))));
}

/* tan(y), or -1/tan(y) when `odd`, for |y| <= pi/4. */
static float tanKernel(double y, int odd)
{
    double s = sinSeries(y);
    double c = cosSeries(y);

    return (float) (odd ? -c / s : s / c);
}

/** tan of a float, alike on every host (the file's comment says how). */
float tanf(float x)
{
    uint32_t bits = floatBits(x);
    uint32_t magnitude = bits & 0x7fffffffu;
    uint32_t quarter = 0;
    double y;
    float result;

    if (magnitude > 0x7f800000u)
    {
        return x + x; /* NaN: quieted, as libm returns it */
    }
    if (magnitude == 0x7f800000u)
    {
        return bitsFloat(0x7fc00000u); /* tan(+-inf): one NaN on every host */
    }
    if (magnitude <= COMPARE_FLOAT_PI_4_BITS)
    {
        return tanKernel((double) x, 0);
    }
    y = reduceQuarterTurns(magnitude, &quarter);
    result = tanKernel(y, (int) (quarter & 1u));
    return (bits & 0x80000000u) ? -result : result;
}

/** cos of a float, alike on every host (the file's comment says how). */
float cosf(float x)
{
    uint32_t magnitude = floatBits(x) & 0x7fffffffu;
    uint32_t quarter = 0;
    double y;

    if (magnitude > 0x7f800000u)
    {
        return x + x; /* NaN: quieted, as libm returns it */
    }
    if (magnitude == 0x7f800000u)
    {
        return bitsFloat(0x7fc00000u); /* cos(+-inf): one NaN on every host */
    }
    if (magnitude <= COMPARE_FLOAT_PI_4_BITS)
    {
        return (float) cosSeries((double) x);
    }
    /* cos is even: cos(|x|) = cos(n pi/2 + y), by n mod 4 */
    y = reduceQuarterTurns(magnitude, &quarter);
    switch (quarter)
    {
    case 0:
        return (float) cosSeries(y);
    case 1:
        return (float) -sinSeries(y);
    case 2:
        return (float) -cosSeries(y);
    default:
        return (float) sinSeries(y);
    }
}

/* atan(u) for |u| <= 7/16 from its Taylor series in u^2 <= 0.2, by Horner's
 * rule from the 20th term (the first omitted one is below 2^-55 of u). */
static double atanSeries(double u)
{
    double z = u * u;
    double sum = 0.0;
    int k;

    for (k = COMPARE_ATAN_TERMS - 1; k >= 0; k--)
    {
        sum = 1.0 / (double) (2 * k + 1) - z * sum;
    }
    return u * sum;
}

/** atan of a float, alike on every host (the file's comment says how). */
float atanf(float x)
{
    double a = (double) x;
    double result;

    if (x != x)
    {
        return x + x; /* NaN: quieted, as libm returns it */
    }
    if (a < 0.0)
    {
        a = -a;
    }
    /* atan(a) = atan(c) + atan((a - c) / (1 + a c)), c chosen by band */
    if (a < 7.0 / 16.0)
    {
        result = atanSeries(a);
    }
    else if (a < 11.0 / 16.0)
    {
        result = COMPARE_ATAN_HALF + atanSeries((2.0 * a - 1.0) / (2.0 + a));
    }
    else if (a < 19.0 / 16.0)
    {
        result = COMPARE_PI_4 + atanSeries((a - 1.0) / (a + 1.0));
    }
    else if (a < 39.0 / 16.0)
    {
        result = COMPARE_ATAN_3_2 + atanSeries((2.0 * a - 3.0) / (2.0 + 3.0 * a));
    }
    else
    {
        result = COMPARE_PI_2 - atanSeries(1.0 / a); /* 1/inf is 0: atan(inf) = pi/2 */
    }
    return (floatBits(x) & 0x80000000u) ? (float) -result : (float) result;
}

#endif
