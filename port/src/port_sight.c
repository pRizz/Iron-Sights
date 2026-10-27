/* Fork vector sights: visibility, profile colours and ADS share one path. */
#include <ultra64.h>
#include <bondconstants.h>
#include <fr.h>
#include "game/player.h"
#include "game/bondview.h"
#include "port_ads.h"
#include "port_hud.h"
#include "port_crosshair.h"
#include "gbi_extension.h"

extern s32 portHudScaleActive(void);

/* One vector sight path for the ring target, always-on cross, and sniper ADS.
 * Keep the original visibility gates and wide-aim coordinate conversion. */
static void portDrawTarget(s32 *gdl, s32 ring, f32 scopeBlend)
{
    extern f32 portAimReachX(void);
    extern f32 g_bondviewSceneWideFactor;
    extern s32 portPlayerCrosshairColor(s32 player);
    Gfx *g = (Gfx *) *gdl;
    f32 x = g_CurrentPlayer->crosshair_angle.x;
    f32 y = g_CurrentPlayer->crosshair_angle.y;
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
    const f32 hudScale = !ring && scopeBlend == 0.0f
        ? portCrosshairSizeScale(portHudScaleActive())
        : (portHudScaleActive() ? 0.5f : 1.0f);
    g = portHudReticle(g, x, y, scaleX * hudScale, hudScale,
        ((u32)(color < 0 ? 0xff3a32 : color) << 8) | alpha, ring);
    if (reach > 1.001f) PORT_HUD_ANCHOR_OFF(g);
    *gdl = (s32)g;
}

void portHudGunSight(s32 *gdl)
{
    if (portAdsScopeBlend() > 0.0f)
    {
        if ((g_CurrentPlayer->gunsightmode & ~GUNSIGHTREASON_NOTAIMING) == 0
            && !g_CurrentPlayer->mpmenuon && !g_CurrentPlayer->bonddead)
            portDrawTarget(gdl, FALSE, portAdsScopeBlend());
        return;
    }
    if (portAdsActive()) return;
    if (g_CurrentPlayer->mpmenuon || g_CurrentPlayer->bonddead) return;
    if (g_CurrentPlayer->gunsightmode == 0)
        portDrawTarget(gdl, TRUE, 0.0f);
    else if (g_CurrentPlayer->gunsightmode == GUNSIGHTREASON_NOTAIMING)
    {
        extern s32 portAlwaysCrosshair(void);
        if (portAlwaysCrosshair()) portDrawTarget(gdl, FALSE, 0.0f);
    }
}
