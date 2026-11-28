#ifndef PORT_DYN_H
#define PORT_DYN_H
#include <ultra64.h>
#include <platform_info.h>

/* The vertex pool's least size a player. A 64-bit build takes 512 KB in
 * every stage (WIN64-004): the owner's play of Dam and Facility used about
 * 1.47 times the automated runs' largest peak, past half of 256 KB, and a
 * full pool stops the game ("Frame data buffer exhausted"). A 32-bit build
 * keeps 256 KB, within its 2 GB address space. */
#if IS_64_BIT
#define PORT_DYN_VERTEX_MIN_PER_PLAYER (512 * 1024)
#else
#define PORT_DYN_VERTEX_MIN_PER_PLAYER (256 * 1024)
#endif

/* Native frame data includes generated arms and their transition depth lists. */
s32 portDynVertexCapacity(s32 inheritedBytes, s32 players);
void *portDynAllocate(s32 count, u32 elementBytes, u32 alignment);
#endif
