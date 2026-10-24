#ifndef PORT_ARM_MESH_H
#define PORT_ARM_MESH_H
#include <ultra64.h>
#include <bondtypes.h>
enum { PORT_ARM_FOREARM=1, PORT_ARM_HAND=2, PORT_ARM_WATCH=4 };
/* Preserve authored mip texture commands under the native pistol material. */
enum { PORT_ARM_NATIVE_SKIN=8 };
typedef void (*PortArmMap)(const coord3d *source, coord3d *target, s32 part, void *context);
typedef struct { Gfx *color, *depth; } PortArmMesh;
typedef struct {
    const ModelRoData_DisplayListRecord *parts[24],*arm;
    s32 count;
} PortGripSource;
const PortGripSource *portArmGripSource(void);
s32 portArmGripSkin(u32 image);
/* Bounds of the shared hand/forearm seam, excluding unrelated palm vertices. */
s32 portArmWristBounds(coord3d *minimum, coord3d *maximum);
s32 portArmWristContains(const coord3d *point);
s32 portArmElbowBounds(s32 cuff, s32 bond, coord3d *minimum, coord3d *maximum);
s32 portArmSectionBounds(s32 cuff, s32 bond, f32 x, coord3d *minimum, coord3d *maximum);
f32 portArmWatchCenter(void);
/* Complete the pause model's open bracelet underside on generated ADS arms. */
s32 portArmMeshBuildWatchBack(PortArmMesh *mesh,s32 cuff,s32 bond,
    PortArmMap map,void *context,f32 light,u8 alpha);
typedef struct { coord3d target; f32 fraction; } PortArmJoinPoint;
/* Intermediate native wrist corners along a source forearm edge, in order. */
typedef s32 (*PortArmJoin)(const coord3d *a, const coord3d *b, PortArmJoinPoint *points, void *context);
/* Retarget only skin UV/RGB on frame-owned copies; geometry/alpha stay fixed. */
typedef void (*PortArmSurface)(const coord3d *source,u32 image,Vtx *vertex,f32 light,void *context);
s32 portArmMeshBuildJoinedSurface(PortArmMesh *mesh,s32 cuff,s32 bond,s32 parts,
    PortArmMap map,void *context,f32 light,u8 alpha,PortArmJoin join,PortArmSurface surface);
s32 portArmMeshBuildJoined(PortArmMesh *mesh, s32 cuff, s32 bond, s32 parts,
    PortArmMap map, void *context, f32 light, u8 alpha, PortArmJoin join);
/* Preserve selected triangles from a mixed native hand/forearm node. Image is
 * the resolved texture address; original positions/UVs and vertex shade are retained.
 * matchSkinTone applies the forearm's correction to the shared wrist materials. */
typedef s32 (*PortArmKeepTriangle)(const Vtx *a,const Vtx *b,const Vtx *c,u32 image,void *context);
s32 portArmMeshBuildFiltered(PortArmMesh *mesh,const ModelRoData_DisplayListRecord *source,
    PortArmMap map,void *context,f32 light,u8 alpha,PortArmKeepTriangle keep,s32 matchSkinTone);
/* Read the watch's own meshes/UVs from the player's ROM, then retarget a
 * frame-owned copy. The callback receives authored positions (wrist X=-180).
 * Opaque draws omit the unused depth-only list; alpha fades receive both. */
s32 portArmMeshBuild(PortArmMesh *mesh, s32 cuff, s32 bond, s32 parts,
    PortArmMap map, void *context, f32 light, u8 alpha);
#endif
