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
 *
 * The pool reports (LP64-004 Task 2): at 64 bits records and display-list
 * commands grow, so some of the game's pools hold less than at 32 bits, and
 * four of them fail without a word (the music heap, room memory, the graphics
 * buffer and the texture pool). At 64 bits each failure calls
 * portLp64PoolExhausted or portLp64PoolFatal, whose line
 * scripts/port/lp64_boot.py treats as a failed boot. At 32 bits they are macros
 * that generate no code, and each pool behaves as it always has.
 *
 * The pool peaks (LP64-005 Task 13): each pool's allocator reports how full it
 * is through portLp64PoolUse, and the largest use of each is printed at each
 * stage unload and at exit, so that the size a pool needs at 64 bits is a
 * measurement. At 32 bits these are macros that generate no code and do not
 * evaluate their arguments, so no argument may have a side effect.
 */
#ifndef PORT_LP64_H
#define PORT_LP64_H

#include <stddef.h>
#include <PR/ultratypes.h>
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

/** True once the class has a 64-bit converter (the table in port_lp64.c). */
int portLp64AssetConverted(enum PortLp64Asset asset);

/**
 * Stops the game with exit status 5 when the class is marked unconverted (an
 * entry of port_lp64.c's table set to 0, or GE_LP64_TEST_UNCONVERTED in a test
 * build), logging: ge: lp64: stopping: asset class "NAME" is marked
 * unconverted in this build. Returns when it is converted.
 */
void portLp64RequireConverter(enum PortLp64Asset asset);

/**
 * Logs: ge: lp64: pool "POOL" exhausted (request N bytes), then returns, for
 * a pool whose failure the game survives as it does at 32 bits. At most eight
 * lines a pool a stage. POOL is one of "music heap", "room memory",
 * "graphics buffer" and "texture pool".
 */
void portLp64PoolExhausted(const char *pool, u32 request);

/** Logs the same line, always, then stops the game with exit status 6. */
void portLp64PoolFatal(const char *pool, u32 request);

/** A stage load begins (port_late_loads.c): every pool may log eight lines again. */
void portLp64PoolStageBegin(void);

/**
 * Keeps the largest USED seen for POOL since the last portLp64PoolPeaks,
 * with its SIZE. A pool name that several pools of other sizes share
 * ("texture pool") keeps the use that was the largest share of its size. POOL
 * is one of "stage bank", "room memory", "room memory largest request",
 * "graphics buffer", "vertex pool", "music heap" and "texture pool", and
 * since LP64-006 Task 11 "permanent bank" (port_memp.c), "gun room left
 * hand", "gun room trigger" and "gun room weapon" (gun.c), "wallet room"
 * (front.c) and "synthesizer parameters" (synthesizer.c), and since Task 11b
 * "high arena" (pcsched.c: portAllocHigh's 64 MB, which only grows, so its
 * use at a print is all it holds since the game started). The first call
 * registers portLp64PoolPeaks to run at exit.
 */
void portLp64PoolUse(const char *pool, u32 used, u32 size);

/**
 * A use that may be given back at once: the stage bank's file loads take all
 * the bank has left and shrink the block to the file once it is in
 * (ob.c, port_memp.c). It counts at the print only if no portLp64PoolUse of
 * POOL came after it; a pool whose use is its cursor reports the shrunk
 * block, or anything kept after it, through that later use.
 */
void portLp64PoolHold(const char *pool, u32 used, u32 size);

/**
 * Logs: ge: lp64: peak POOL USED of SIZE bytes, for each pool used since the
 * last call, in the order of first use, then forgets them; a room block still
 * held whole (portLp64RoomTake) and a held use count as kept. Called at each
 * stage unload (lv.c) and at exit.
 */
void portLp64PoolPeaks(void);

/**
 * Room memory's use (mema.c), which portLp64PoolUse is told of as "room
 * memory" and "room memory largest request". A room's first load takes the
 * largest free block (memaGetLongestFree) and gives back its tail once the
 * room is in it (bg.c); that block counts as what it kept, not as the free
 * heap it briefly held. portLp64RoomReset: the heap of SIZE bytes is empty.
 * portLp64RoomLongest: memaGetLongestFree answered BYTES. portLp64RoomTake:
 * BYTES at ADDR were allocated (memaAlloc) or added to the block ending there
 * (memaGrow). portLp64RoomGive: BYTES at ADDR were freed.
 */
void portLp64RoomReset(u32 size);
void portLp64RoomLongest(u32 bytes);
void portLp64RoomTake(uintptr_t addr, u32 bytes);
void portLp64RoomGive(uintptr_t addr, u32 bytes);

/**
 * The synthesizer's parameter pool (synthesizer.c, LP64-006 Task 11), which
 * portLp64PoolUse is told of as "synthesizer parameters" in bytes:
 * portLp64SynthParams: the pool of RECORDS records of RECORD_BYTES each is
 * made, all free (alSynNew). portLp64SynthParamTake: a record left the free
 * list (__allocParam). portLp64SynthParamGive: one came back (__freeParam).
 */
void portLp64SynthParams(u32 records, u32 recordBytes);
void portLp64SynthParamTake(void);
void portLp64SynthParamGive(void);

#ifdef __cplusplus
}
#endif
#else
#define portLp64RegionIsHigh(start, size) 1
#define portLp64CheckRegion(name, start, size) ((void) 0)
#define portLp64CheckAddressSpace() ((void) 0)
#define portLp64AssetConverted(asset) 1
#define portLp64RequireConverter(asset) ((void) 0)
#define portLp64PoolExhausted(pool, request) ((void) 0)
#define portLp64PoolFatal(pool, request) ((void) 0)
#define portLp64PoolStageBegin() ((void) 0)
#define portLp64PoolUse(pool, used, size) ((void) 0)
#define portLp64PoolHold(pool, used, size) ((void) 0)
#define portLp64PoolPeaks() ((void) 0)
#define portLp64RoomReset(size) ((void) 0)
#define portLp64RoomLongest(bytes) ((void) 0)
#define portLp64RoomTake(addr, bytes) ((void) 0)
#define portLp64RoomGive(addr, bytes) ((void) 0)
#define portLp64SynthParams(records, recordBytes) ((void) 0)
#define portLp64SynthParamTake() ((void) 0)
#define portLp64SynthParamGive() ((void) 0)
#endif

/* __allocParam's result (synthesizer.c): a comparison build counts each update it
 * dropped for want of a free record, and prints the count at exit
 * (port_compare_math.c, LP64-006 final review); elsewhere nothing. */
#ifdef GE_COMPARE
void portCompareSynthDropped(void);
#define portCompareSynthTaken(update) ((update) == 0 ? portCompareSynthDropped() : (void) 0)
#else
#define portCompareSynthTaken(update) ((void) 0)
#endif

#if IS_64_BIT || defined(GE_LP64_ORACLE)
#ifdef __cplusplus
extern "C" {
#endif
/** The class's name as the guard prints it ("animation tables");
 * "unknown asset class" for a value outside the enum. Also in the 32-bit
 * oracle build (-DGE_LP64_ORACLE=ON), whose converters and oracle lines name
 * classes; a shipped 32-bit build has no use for it. */
const char *portLp64AssetName(enum PortLp64Asset asset);
#ifdef __cplusplus
}
#endif
#endif

#endif
