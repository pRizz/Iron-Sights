/* The stage pools' 64-bit sizes set from the soak (LP64-006 Task 11;
 * port_lp64_budget.h). Each row: a pool whose largest peak over the Step 3
 * runs was above half the size its stage gave it, raised to twice that peak
 * rounded up to 16 (every pool here is carved in 16-byte units at 64 bits).
 * The figures and their cases are in scripts/port/lp64_budgets.txt. No code
 * at 32 bits (CMakeLists.txt leaves the file out). */
#include "port_lp64_budget.h"
#include <bondconstants.h>

#if IS_64_BIT
typedef struct StageBudget
{
    const char *pool;
    s32 level;
    u32 size;   /* what the stage's token gives the pool at 64 bits */
    u32 raised; /* twice the peak, rounded up to 16 */
} StageBudget;

static const StageBudget sBudgets[] = {
    /* room memory: peak 158,304 bytes (soak, headless) */
    { "room memory", LEVELID_SILO, 307200, 316608 },
    /* peak 271,120 (tour-statue); the single-player token, multiplayer's is LP64-005's 839,200 */
    { "room memory", LEVELID_STATUE, 450560, 542240 },
    /* peak 188,576 (tour-aztec) */
    { "room memory", LEVELID_AZTEC, 276480, 377152 },
    /* peak 308,384 (tour-dam) */
    { "room memory", LEVELID_DAM, 563200, 616768 },
    /* peak 228,336 (mp-caverns-4p, video); the multiplayer token */
    { "room memory", LEVELID_CAVERNS, 450560, 456672 },
    /* peak 239,360 (tour-jungle, ruling 9's plain sets; Step 3's tour gave 202,624) */
    { "room memory", LEVELID_JUNGLE, 409600, 478720 },
    /* peak 265,456 (respawn-cradle, the same) */
    { "room memory", LEVELID_CRADLE, 512000, 530912 },
    /* graphics buffer: peak 125,520 (stage-aztec, comparison build with video, every fork option) */
    { "graphics buffer", LEVELID_AZTEC, 245760, 251040 },
    /* vertex pool: peak 135,136 (stage-caverns, the same), one player */
    { "vertex pool", LEVELID_CAVERNS, 262144, 270272 },
};

/* Not strcmp: the game's <string.h> (include/) does not declare it. */
static int sameName(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }
    return *a == *b;
}

u32 portLp64StageBudget(const char *pool, s32 level, u32 size)
{
    size_t i;

    for (i = 0; i < sizeof(sBudgets) / sizeof(sBudgets[0]); i++)
    {
        if (sBudgets[i].level == level && sBudgets[i].size == size && sameName(sBudgets[i].pool, pool))
        {
            return sBudgets[i].raised;
        }
    }
    return size;
}
#endif
