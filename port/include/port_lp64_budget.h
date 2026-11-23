/**
 * The stage pools' 64-bit sizes set from the soak (LP64-006 Task 11,
 * decision 8): at 64 bits every pool in every stage is at least twice its
 * largest measured peak. A stage's token (src/boss.c, memallocstringtable)
 * gives it room memory, a graphics buffer and a vertex pool; where one of
 * them peaked above half its size, port_lp64_budget.c holds the size it gets
 * instead, keyed by the pool's name as its peak line prints it, the level and
 * the size the stage gave it (so a multiplayer token, or the vertex pool of
 * another player count, keeps its own). scripts/port/lp64_budgets.txt lists
 * every pool, level and size with its peak; `lp64_play.py peaks --budget`
 * checks runs against it. At 32 bits portLp64StageBudget is SIZE, a macro
 * that does not evaluate POOL or LEVEL.
 */
#ifndef PORT_LP64_BUDGET_H
#define PORT_LP64_BUDGET_H

#include <PR/ultratypes.h>
#include "platform_info.h"

#if IS_64_BIT
#ifdef __cplusplus
extern "C" {
#endif
/** SIZE, or the size the table gives POOL in LEVEL when the stage gave it SIZE. */
u32 portLp64StageBudget(const char *pool, s32 level, u32 size);
#ifdef __cplusplus
}
#endif
#else
#define portLp64StageBudget(pool, level, size) (size)
#endif

#endif
