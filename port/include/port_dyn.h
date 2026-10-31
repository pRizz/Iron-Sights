#ifndef PORT_DYN_H
#define PORT_DYN_H
#include <ultra64.h>

/* Native frame data includes generated arms and their transition depth lists. */
s32 portDynVertexCapacity(s32 inheritedBytes, s32 players);
void *portDynAllocate(s32 count, u32 elementBytes, u32 alignment);
#endif
