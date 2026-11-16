/**
 * The 64-bit address rule (scripts/port/native_64bit_plan.md, "Addresses"):
 * at 64 bits PORT_IS_HOST_ADDR (port_hostptr.h) calls any value with one of its
 * upper 32 bits set a host pointer, which holds only while nothing the game
 * addresses sits below 4 GB. These functions check that: each allocator passes
 * its region to portLp64CheckRegion, and main() runs portLp64CheckAddressSpace
 * once the arenas and the ROM image are in place.
 *
 * At 32 bits every address is below 4 GB and the rule does not apply: all
 * three are macros that generate no code.
 */
#ifndef PORT_LP64_H
#define PORT_LP64_H

#include <stddef.h>
#include "platform_info.h"

#if IS_64_BIT
#ifdef __cplusplus
extern "C" {
#endif

/** True when the whole region [start, start + size) is at or above 4 GB. Pure. */
int portLp64RegionIsHigh(const void *start, size_t size);

/**
 * Stops the game with exit status 4, naming NAME, when the region is not
 * wholly above 4 GB; otherwise remembers it for portLp64CheckAddressSpace's
 * report.
 */
void portLp64CheckRegion(const char *name, const void *start, size_t size);

/**
 * Checks the main executable's loaded segments, a malloc block and a stack
 * address the same way, then logs "ge: lp64: address space OK" with the name
 * and start of each remembered region.
 */
void portLp64CheckAddressSpace(void);

#ifdef __cplusplus
}
#endif
#else
#define portLp64RegionIsHigh(start, size) 1
#define portLp64CheckRegion(name, start, size) ((void) 0)
#define portLp64CheckAddressSpace() ((void) 0)
#endif

#endif
