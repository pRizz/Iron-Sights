/**
 * The 64-bit address rule (scripts/port/native_64bit_plan.md, "Addresses"):
 * at 64 bits PORT_IS_HOST_ADDR (port_hostptr.h) calls any value with one of its
 * upper 32 bits set a host pointer, which holds only while nothing the game
 * addresses sits below 4 GB. These functions check that: each allocator passes
 * its region to portLp64CheckRegion, and main() runs portLp64CheckAddressSpace
 * once the arenas and the ROM image are in place.
 *
 * At 32 bits every address is below 4 GB and the rule does not apply: those
 * three are macros that generate no code.
 *
 * The asset-class guard (LP64-003 Task 4): the ten classes of ROM data the
 * design ("Converters") converts with a converter per class. Today each class
 * is converted in place by code that assumes 4-byte pointers, so at 64 bits
 * the first statement of each in-place conversion is
 * portLp64RequireConverter(class), which stops the game, named, unless the
 * class is marked converted in port_lp64.c. LP64-004 marks a class there in
 * the commit that lands its converter. At 32 bits the in-place conversions
 * are the shipped path: the guard is a macro that generates no code and every
 * class counts as converted.
 */
#ifndef PORT_LP64_H
#define PORT_LP64_H

#include <stddef.h>
#include "platform_info.h"

/** The design's asset classes, in the design's order (not the boot's). */
enum PortLp64Asset
{
    PORT_LP64_TEXT_BANKS,
    PORT_LP64_FONTS,
    PORT_LP64_TEXTURES,
    PORT_LP64_ANIMATIONS,
    PORT_LP64_MODELS,
    PORT_LP64_COLLISION,
    PORT_LP64_SETUPS,
    PORT_LP64_BACKGROUNDS,
    PORT_LP64_AUDIO_BANKS,
    PORT_LP64_MUSIC,
    PORT_LP64_ASSET_COUNT
};

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

/** The class's name as the guard prints it ("animation tables");
 * "unknown asset class" for a value outside the enum. */
const char *portLp64AssetName(enum PortLp64Asset asset);

/** True once the class has a 64-bit converter (the table in port_lp64.c). */
int portLp64AssetConverted(enum PortLp64Asset asset);

/**
 * Stops the game with exit status 5 when the class has no 64-bit converter,
 * logging: ge: lp64: stopping: asset class "NAME" has no 64-bit converter yet
 * (LP64-004). Returns when it has one.
 */
void portLp64RequireConverter(enum PortLp64Asset asset);

#ifdef __cplusplus
}
#endif
#else
#define portLp64RegionIsHigh(start, size) 1
#define portLp64CheckRegion(name, start, size) ((void) 0)
#define portLp64CheckAddressSpace() ((void) 0)
#define portLp64AssetConverted(asset) 1
#define portLp64RequireConverter(asset) ((void) 0)
#endif

#endif
