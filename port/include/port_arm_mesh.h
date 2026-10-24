#ifndef PORT_ARM_MESH_H
#define PORT_ARM_MESH_H
#include <ultra64.h>
#include <bondtypes.h>
enum { PORT_ARM_FOREARM=1, PORT_ARM_HAND=2, PORT_ARM_WATCH=4 };
typedef void (*PortArmMap)(const coord3d *source, coord3d *target, s32 part, void *context);
typedef struct { Gfx *color, *depth; } PortArmMesh;
/* Coordinated wrist/palm/forearm profile, including a matching watch refit. */
f32 portArmForearmBulk(f32 sourceX);
/* Bounds of the shared hand/forearm seam, excluding unrelated palm vertices. */
s32 portArmWristBounds(coord3d *minimum, coord3d *maximum);
s32 portArmWristContains(const coord3d *point);
s32 portArmElbowBounds(s32 cuff, s32 bond, coord3d *minimum, coord3d *maximum);
s32 portArmSectionBounds(s32 cuff, s32 bond, f32 x, coord3d *minimum, coord3d *maximum);
f32 portArmWatchCenter(void);
typedef struct { coord3d target; f32 fraction; } PortArmJoinPoint;
/* Intermediate native wrist corners along a source forearm edge, in order. */
typedef s32 (*PortArmJoin)(const coord3d *a, const coord3d *b, PortArmJoinPoint *points, void *context);
s32 portArmMeshBuildJoined(PortArmMesh *mesh, s32 cuff, s32 bond, s32 parts,
    PortArmMap map, void *context, f32 light, u8 alpha, PortArmJoin join);
/* Read the watch's own meshes/UVs from the player's ROM, then retarget a
 * frame-owned copy. The callback receives authored positions (wrist X=-180).
 * Opaque draws omit the unused depth-only list; alpha fades receive both. */
s32 portArmMeshBuild(PortArmMesh *mesh, s32 cuff, s32 bond, s32 parts,
    PortArmMap map, void *context, f32 light, u8 alpha);
#endif
