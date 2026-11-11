/* Fork vector sights: visibility, profile colours and ADS share one path. */
#include <ultra64.h>
#include <bondconstants.h>
#include <fr.h>
#include "game/player.h"
#include "game/bondview.h"
#include "port_ads.h"
#include "port_hud.h"
#include "port_crosshair.h"
#include "port_projectile_arc.h"
#include "gbi_extension.h"

extern s32 portHudScaleActive(void);

/* One vector sight path for the ring target, always-on cross, and sniper ADS.
 * Keep the original visibility gates and wide-aim coordinate conversion.
 * at: where to draw it (crosshair_angle's coordinates), NULL for the aim
 * point; at[2] >= 0: something hides that impact, so the cross is fainter
 * and dashed, its dashes marching outward with that phase; at[3]: its size
 * for the impact's distance (FORK-069). */
static void portDrawTarget(s32 *gdl, s32 ring, f32 scopeBlend, const f32 *at)
{
    extern f32 portAimReachX(void);
    extern f32 g_bondviewSceneWideFactor;
    extern s32 portPlayerCrosshairColor(s32 player);
    Gfx *g = (Gfx *) *gdl;
    f32 x = (at != NULL) ? at[0] : g_CurrentPlayer->crosshair_angle.x;
    f32 y = (at != NULL) ? at[1] : g_CurrentPlayer->crosshair_angle.y;
    f32 scaleX = 1.0f;
    const f32 reach = portAimReachX();
    s32 color = portPlayerCrosshairColor(get_cur_playernum());
    u32 alpha = 220;
    if (scopeBlend > 0.0f)
    {
        x = viGetViewLeft() + viGetViewWidth() * 0.5f;
        y = viGetViewTop() + viGetViewHeight() * 0.5f;
        color = -1; /* the sniper optic retains its red aiming mark */
        alpha = (u32)(alpha * scopeBlend * scopeBlend);
    }
    if (reach > 1.001f)
    {
        PORT_HUD_ANCHOR(g, PORT_HUD_ANCHOR_SPAN);
        x = bondviewAimUnpinX(x);
        scaleX = 1.0f / g_bondviewSceneWideFactor;
    }
    const f32 hudScale = (!ring && scopeBlend == 0.0f
        ? portCrosshairSizeScale(portHudScaleActive())
        : (portHudScaleActive() ? 0.5f : 1.0f)) * (at != NULL ? at[3] : 1.0f);
    if (at != NULL && at[2] >= 0.0f)
        g = portHudReticleDashed(g, x, y, scaleX * hudScale, hudScale,
            ((u32)(color < 0 ? 0xff3a32 : color) << 8) | (alpha * 2 / 3), at[2]);
    else
        g = portHudReticle(g, x, y, scaleX * hudScale, hudScale,
            ((u32)(color < 0 ? 0xff3a32 : color) << 8) | alpha, ring);
    if (reach > 1.001f) PORT_HUD_ANCHOR_OFF(g);
    *gdl = (s32)g;
}

/* The always-on cross: at each arc's impact while the Arc Impact Crosshair
 * (FORK-069) has one, else at the aim point. */
static void portDrawAlwaysTarget(s32 *gdl)
{
    f32 impacts[PORT_ARC_SLOTS][4];
    const s32 n = portProjectileArcCrosshairs(impacts);
    s32 i;

    if (n == 0) portDrawTarget(gdl, FALSE, 0.0f, NULL);
    for (i = 0; i < n; i++) portDrawTarget(gdl, FALSE, 0.0f, impacts[i]);
}

void portHudGunSight(s32 *gdl)
{
    if (portAdsScopeBlend() > 0.0f)
    {
        if ((g_CurrentPlayer->gunsightmode & ~GUNSIGHTREASON_NOTAIMING) == 0
            && !g_CurrentPlayer->mpmenuon && !g_CurrentPlayer->bonddead)
            portDrawTarget(gdl, FALSE, portAdsScopeBlend(), NULL);
        return;
    }
    if (g_CurrentPlayer->mpmenuon || g_CurrentPlayer->bonddead) return;
    if (portAdsActive())
    {
        /* ADS Crosshair (FORK-060, debugging): the always-on cross at the aim
         * point over the sights, to check them against it */
        if (portCrosshairAds()) portDrawAlwaysTarget(gdl);
        return;
    }
    if (g_CurrentPlayer->gunsightmode == 0)
        portDrawTarget(gdl, TRUE, 0.0f, NULL);
    else if (g_CurrentPlayer->gunsightmode == GUNSIGHTREASON_NOTAIMING)
    {
        extern s32 portAlwaysCrosshair(void);
        if (portAlwaysCrosshair()) portDrawAlwaysTarget(gdl);
    }
}
