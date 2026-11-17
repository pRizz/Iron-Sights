/**
 * The load census's lines (romconv_census.h; LP64-004 Task 16): which model
 * node opcodes and setup prop record types a run converted, for
 * scripts/port/lp64_census.py's `types`. Compiled only at 64 bits and in the
 * 32-bit oracle build.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ultra64.h>
#include "romconv_census.h"

extern void portConLogf(const char *fmt, ...);

int portCensusOn(void)
{
    static int on = -1;

    if (on < 0)
    {
        const char *value = getenv("GE_LP64_CENSUS");

        on = value != NULL && value[0] == '1' && value[1] == '\0';
    }
    return on;
}

void portCensusLine(const char *what, const char *name, const char *label, const u32 *counts, u32 n)
{
    char line[2048];
    int used;
    u32 i;

    if (!portCensusOn())
    {
        return;
    }
    used = snprintf(line, sizeof(line), "ge: census: %s %s %s", what, name, label);
    for (i = 0; i < n && used > 0 && (size_t) used < sizeof(line); i++)
    {
        if (counts[i] != 0)
        {
            used += snprintf(line + used, sizeof(line) - (size_t) used, " %u=%u", i, counts[i]);
        }
    }
    portConLogf("%s\n", line);
}
