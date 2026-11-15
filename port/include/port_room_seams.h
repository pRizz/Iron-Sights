#ifndef PORT_ROOM_SEAMS_H
#define PORT_ROOM_SEAMS_H
#include <ultra64.h>

/* bg.c, around room `room`'s opaque display list (FORK-086): G_NOOP tag 0x68
 * on / off, naming the room and the stage. Inside the span the renderer gives
 * each opaque triangle a backing copy, grown along its unshared edges and
 * drawn behind the real surfaces, covering the seams in the level's meshes
 * that showed the sky behind them (GE_SEAM_GROW sets the grow, 0 = off). */
Gfx *portRoomSeamsTag(Gfx *gdl, s32 on, s32 room);

#endif
