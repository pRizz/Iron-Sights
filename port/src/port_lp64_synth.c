/* The synthesizer's parameter pool's peak (LP64-006 Task 11; port_lp64.h).
 * synthesizer.c builds a free list of update records at boot (alSynNew) and
 * takes and returns them as sounds start, change and stop; a record leaves the
 * list only in __allocParam and comes back only in __freeParam (env.c frees
 * each update it applies). When the list is empty __allocParam returns NULL
 * and the update is dropped. The records in use are counted here and told to
 * portLp64PoolUse in bytes, so the pool's peak prints with the others. Each
 * take is one table search (port_lp64.c, by pointer), several times a frame
 * while sound plays; a 32-bit build has no code here (CMakeLists.txt leaves
 * this file out, and the calls are macros). Split from port_lp64.c, which
 * was past 500 lines. */
#include "port_lp64.h"

#if IS_64_BIT
static u32 s_ParamRecords, s_ParamBytes, s_ParamsInUse;

void portLp64SynthParams(u32 records, u32 recordBytes)
{
    s_ParamRecords = records;
    s_ParamBytes = recordBytes;
    s_ParamsInUse = 0;
}

void portLp64SynthParamTake(void)
{
    s_ParamsInUse++;
    portLp64PoolUse("synthesizer parameters", s_ParamsInUse * s_ParamBytes, s_ParamRecords * s_ParamBytes);
}

void portLp64SynthParamGive(void)
{
    if (s_ParamsInUse > 0)
    {
        s_ParamsInUse--;
    }
}
#endif
