/**
 * The load census's lines (LP64-004 Task 16), printed by the 64-bit model and
 * setup converters and read by scripts/port/lp64_census.py. Compiled where
 * the converters are (64 bits and the 32-bit oracle build).
 */
#ifndef ROMCONV_CENSUS_H
#define ROMCONV_CENSUS_H

#include <PR/ultratypes.h>

/** Whether GE_LP64_CENSUS=1 asks for the census lines (read once). */
int portCensusOn(void);

/**
 * Logs `ge: census: WHAT NAME LABEL i=COUNTS[i] ...` for each of the N counts
 * that is not 0, when portCensusOn(); nothing otherwise.
 */
void portCensusLine(const char *what, const char *name, const char *label, const u32 *counts, u32 n);

#endif
