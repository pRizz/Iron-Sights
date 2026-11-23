/* Float-to-integer conversions whose result must not follow the host
 * (LP64-006 Task 14). Included once, by probe.c. */
#ifndef PROBE_CONVERSIONS_H
#define PROBE_CONVERSIONS_H

#include <limits.h>
#include <stdio.h>
#include <PR/gu.h>

/* gbi.h uses FTOFIX32 where a constant expression is needed. */
_Static_assert(FTOFIX32(0.5f) == 0x8000, "FTOFIX32 is a constant expression (gbi.h's G_DEPTOZSrg)");

/* FTOFIX32 (gu.h) past +-32768.0 is out of int's range: x86's conversion gives
 * INT_MIN either way and Arm's saturates, so guMtxF2L's matrices would follow
 * the host. Every host gives x86's INT_MIN. The volatile keeps the compiler
 * from folding the conversion. */
static int checkFixedPointPastRange(void)
{
    volatile float above = 40000.0f;
    volatile float below = -40000.0f;
    int fixedAbove = FTOFIX32(above);
    int fixedBelow = FTOFIX32(below);
    int failures = (fixedAbove != INT_MIN) + (fixedBelow != INT_MIN);

    if (failures != 0)
    {
        printf("ABI probe FAIL: FTOFIX32(40000.0f) is %d and FTOFIX32(-40000.0f) is %d, not INT_MIN (%d cases)\n",
               fixedAbove, fixedBelow, failures);
    }
    return failures;
}

#endif
