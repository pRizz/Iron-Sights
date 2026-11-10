#ifndef PORT_BULLET_ROOMS_H
#define PORT_BULLET_ROOMS_H
#include <ultra64.h>
#include <bondtypes.h>

/* Wall bullet holes that stay visible while their wall is (FORK-068).
 *
 * Upstream files a wall hole under one room and draws it only while that room
 * is drawn. A wall surface can exist in more than one room, so a hole filed
 * under a room that drops out of view vanished from a wall still on screen.
 * The port also draws the hole while any drawn room has geometry right at the
 * hole; each room is tested once per hole, the first time it is drawn while
 * the hole's own room is not. */

/* chrprop.c, just before a wall hole is created: the hit point and surface
 * normal (world units) for the next hole. */
void portBulletImpactNoteWall(const coord3d *hitpos, const coord3d *normal);

/* explosion.c, as slot is filled: attach the noted wall, or none for a prop. */
void portBulletImpactAssign(s32 slot, PropRecord *prop);

/* explosion.c render: is room, or any drawn room holding slot's wall, drawn? */
s32 portBulletImpactRoomDrawn(s32 slot, s32 room);

#endif
