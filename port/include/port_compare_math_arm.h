/**
 * Shared by the comparison build's two files of Arm's maths (MAC-001 Task 8,
 * FORK-093): port/src/port_compare_math_arm.c (expf and powf) and
 * port/src/port_compare_math_arm_sinf.c (sinf). It holds the refusals that
 * keep their double arithmetic alike on every host, and the helpers of
 * Arm's math_config.h and math_errf.c that both use. port_compare_math_arm.c
 * says where the code comes from and what was changed.
 *
 *   Copyright (c) 2017-2026, Arm Limited.
 *   SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 *
 * Used here under the MIT licence (tools/THIRD_PARTY_NOTICES.txt, "Arm
 * Optimized Routines"). Included only by those two files, which CMake
 * compiles only with -DGE_COMPARE=ON.
 */
#ifndef PORT_COMPARE_MATH_ARM_H
#define PORT_COMPARE_MATH_ARM_H

#include <stdint.h>

#if defined(__i386__) && !defined(__SSE2_MATH__)
#error "the comparison build's maths needs SSE2 double arithmetic (-msse2 -mfpmath=sse): x87 rounds to 80 bits"
#endif
#if defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ != 0
#error "the comparison build's maths needs FLT_EVAL_METHOD 0: every double operation rounded to double"
#endif
#ifdef __FAST_MATH__
#error "the comparison build's maths must not be built with -ffast-math: it reorders the arithmetic"
#endif

/* No <math.h>: the tree's include/math.h shadows the system's. */
#define ARM_INFINITY __builtin_inff()

static inline uint32_t asuint(float f)
{
    union
    {
        float    f;
        uint32_t i;
    } u = {f};
    return u.i;
}

static inline float asfloat(uint32_t i)
{
    union
    {
        uint32_t i;
        float    f;
    } u = {i};
    return u.f;
}

static inline uint64_t asuint64(double f)
{
    union
    {
        double   f;
        uint64_t i;
    } u = {f};
    return u.i;
}

static inline double asdouble(uint64_t i)
{
    union
    {
        uint64_t i;
        double   f;
    } u = {i};
    return u.f;
}

/* math_errf.c: an invalid argument gives a NaN. */
static inline float invalidf(float x)
{
    return (x - x) / (x - x);
}

#endif
