/* Arc Impact Crosshair (FORK-069, [display] arc_crosshair, on by default):
 * while the always-on crosshair is drawn (Always Crosshair, or ADS Crosshair
 * in ADS) and the player holds a weapon with a Projectile Arc, the cross
 * marks where the shot will land instead of the aim point: one for each
 * hand's arc, eased so a jumping impact glides, kept at the view's edge on
 * the line towards an impact outside it. Where the level, an object or a
 * door hides the impact from the eye, the cross is drawn fainter and dashed,
 * its dashes marching outward. It shrinks with the impact's distance, as an
 * object there would, down to a floor that keeps it visible. With no impact (a shot past the
 * prediction's reach) or one behind the camera it stays at the aim point.
 * The arcs are predicted for it even with Projectile Arc itself off; they
 * are only drawn when that is on. port_sight.c asks for the positions. */
#include <math.h>
#include <ultra64.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/player.h"
#include "port_projectile_arc.h"
#include "port_crosshair.h"

extern s32 portAlwaysCrosshair(void);
extern s32 get_cur_playernum(void);

#define ARC_CURSOR_MARGIN 6.0f      /* from the view's edge, screen units */
#define ARC_CURSOR_TICKS  46875000.0 /* osGetTime counts a second */
#define ARC_CURSOR_MARCH  4.0       /* a hidden cross's dashes, native units a second */

typedef struct ArcCursor {
    s32 shown;
    f32 x;
    f32 y;
    f32 scale;
    OSTime at;
} ArcCursor;

static ArcCursor sCursor[4][PORT_ARC_SLOTS];

/* Whether anything needs this player's arcs predicted: the arc drawn, or
 * the crosshair placed on its impact. */
s32 portProjectileArcWanted(void)
{
    return portProjectileArcEnabled()
        || (portArcCrosshairEnabled() && (portAlwaysCrosshair() || portCrosshairAds()));
}

/* The impact in the crosshair's own coordinates (crosshair_angle: pinned 2D,
 * kept inside the view); FALSE when it lies behind the camera. */
static s32 arcCursorProject(const coord3d *p, f32 *x, f32 *y)
{
    Mtxf *w2s = camGetWorldToScreenMtxf();
    coord3d cam;
    coord2d screen;
    s32 i;

    for (i = 0; i < 3; i++)
    {
        cam.f[i] = p->x * w2s->m[0][i] + p->y * w2s->m[1][i] + p->z * w2s->m[2][i] + w2s->m[3][i];
    }
    if (!(cam.z < -1.0f)) return FALSE;            /* the camera looks down -z */
    transform3Dto2DCoords(&cam, &screen);
    portArcCursorClamp(&screen.x, &screen.y, getPlayer_c_screenleft(), getPlayer_c_screentop(),
                       getPlayer_c_screenwidth(), getPlayer_c_screenheight(), ARC_CURSOR_MARGIN);
    *x = bondviewAimPinX(screen.x);
    *y = screen.y;
    return TRUE;
}

/* port_sight.c, each frame the always-on crosshair is drawn: where to draw
 * it for each of the current player's arcs with an impact. Returns how
 * many; 0 draws it at the aim point as usual. */
s32 portProjectileArcCrosshairs(f32 xy[PORT_ARC_SLOTS][4])
{
    ArcCursor *cursor = sCursor[get_cur_playernum() & 3];
    const coord3d *eye = bondviewGetCurrentPlayersPosition();
    const OSTime now = osGetTime();
    s32 count = 0;
    s32 slot;

    for (slot = 0; slot < PORT_ARC_SLOTS; slot++)
    {
        ArcCursor *c = &cursor[slot];
        PortArcLaunch launch;
        const PortArcPath *path = portArcCrosshairEnabled() ? portProjectileArcLast(slot, &launch) : NULL;
        f32 x, y, scale, unused = 0.0f;

        if (path == NULL || !path->impact || !arcCursorProject(&path->end, &x, &y))
        {
            c->shown = FALSE;
            continue;
        }
        {
            const f32 dx = path->end.x - eye->x, dy = path->end.y - eye->y, dz = path->end.z - eye->z;

            scale = portArcCursorScale(sqrtf(dx * dx + dy * dy + dz * dz));
        }
        if (c->shown)
        {   /* position and size glide together */
            const f32 seconds = (f32) ((double) (now - c->at) / ARC_CURSOR_TICKS);

            portArcCursorEase(&c->x, &c->y, x, y, seconds);
            portArcCursorEase(&c->scale, &unused, scale, 0.0f, seconds);
        }
        else { c->x = x; c->y = y; c->scale = scale; }
        c->shown = TRUE;
        c->at = now;
        xy[count][0] = c->x;
        xy[count][1] = c->y;
        xy[count][2] = (path->endHidden || portProjectileArcAuditHidden())
            ? (f32) ((double) (now % (1000ull * 46875000ull)) / ARC_CURSOR_TICKS * ARC_CURSOR_MARCH) : -1.0f;
        xy[count][3] = c->scale;
        count++;
    }
    return count;
}
