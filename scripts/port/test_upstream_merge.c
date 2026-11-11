/* ROM-free integration checks for upstream projection and fork sight policies. */
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../../src/game/gunfire.c"
#include "../../port/src/port_sight.c"
#include "../../port/src/port_crosshair.c"

static struct player player;
struct player *g_CurrentPlayer = &player;
f32 g_bondviewSceneWideFactor = 2.0f;
static f32 baseFov = 90.0f, adsBlend, scopeBlend, reach = 1.0f;
static s32 weaponFov = 1, hudScale, calls, ringDrawn, dashedDrawn;
static f32 drawnX, drawnY, drawnScaleX, drawnScaleY;
static u32 drawnColor;

f32 portFovDefault(void) { return baseFov; }
s32 portWeaponFov(void) { return weaponFov; }
f32 portAdsBlend(void) { return adsBlend; }
f32 portAdsScopeBlend(void) { return scopeBlend; }
s32 portAdsActive(void) { return adsBlend > 0.0f; }
s32 get_cur_playernum(void) { return 0; }
f32 portAimReachX(void) { return reach; }
f32 bondviewAimUnpinX(f32 x) { return x * 2.0f; }
s32 portPlayerCrosshairColor(s32 playerNum) { return 0x123456; }
s32 portAlwaysCrosshair(void) { return 1; }
s32 portHudScaleActive(void) { return hudScale; }
s16 viGetViewLeft(void) { return 20; }
s16 viGetViewTop(void) { return 10; }
s16 viGetViewWidth(void) { return 320; }
s16 viGetViewHeight(void) { return 240; }
Gfx *portHudReticle(Gfx *g, f32 x, f32 y, f32 sx, f32 sy, u32 color, s32 ring)
{
    calls++;
    drawnX = x; drawnY = y; drawnScaleX = sx; drawnScaleY = sy;
    drawnColor = color; ringDrawn = ring;
    return g;
}
Gfx *portHudReticleDashed(Gfx *g, f32 x, f32 y, f32 sx, f32 sy, u32 color, f32 phase)
{
    g = portHudReticle(g, x, y, sx, sy, color, 0);
    dashedDrawn = phase >= 0.0f;
    return g;
}
Gfx *portHudPanel(Gfx *g, f32 x, f32 y, f32 w, f32 h, u32 color) { return g; }
/* Arc Impact Crosshair (FORK-069): the impacts it reports this frame. */
static s32 arcImpacts;
static f32 arcImpactAt[PORT_ARC_SLOTS][4] = { { 50.0f, 60.0f, -1.0f, 1.0f }, { 70.0f, 80.0f, -1.0f, 1.0f } };
s32 portProjectileArcCrosshairs(f32 xy[PORT_ARC_SLOTS][4])
{
    s32 i, k;

    for (i = 0; i < arcImpacts; i++) for (k = 0; k < 4; k++) xy[i][k] = arcImpactAt[i][k];
    return arcImpacts;
}
void osSyncPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format); vfprintf(stderr, format, args); va_end(args);
    abort();
}

static void projectionChecks(void)
{
    const f32 bases[] = {60.0f, 90.0f, 110.0f, 120.0f};
    s32 i;
    for (i = 0; i < 4; i++) {
        f32 hip;
        baseFov = bases[i]; adsBlend = 0.0f;
        hip = gunWeaponFovY(baseFov);
        assert(hip > 59.99f && hip < 60.01f);
        assert(gunWeaponFovY(4.6f) == 4.6f); /* watch text stays on its face */
        adsBlend = 0.5f;
        assert(gunWeaponFovY(baseFov) > (hip + baseFov) * 0.5f - 0.001f);
        assert(gunWeaponFovY(baseFov) < (hip + baseFov) * 0.5f + 0.001f);
        adsBlend = 1.0f;
        assert(gunWeaponFovY(baseFov) == baseFov);
        assert(gunWeaponFovY(35.0f) == 35.0f); /* calibrated ADS zoom */
        weaponFov = 0; adsBlend = 0.0f;
        assert(gunWeaponFovY(baseFov) == baseFov);
        weaponFov = 1;
    }
}

static void sightChecks(void)
{
    Gfx commands[16];
    s32 gdl = (s32)commands;
    player.crosshair_angle.x = 100.25f; player.crosshair_angle.y = 90.5f;
    portHudGunSight(&gdl);
    assert(calls == 1 && ringDrawn && drawnX == 100.25f && drawnY == 90.5f);
    assert(drawnScaleX == 1.0f && drawnColor == 0x123456dc);
    hudScale = 1; reach = 2.0f; player.gunsightmode = GUNSIGHTREASON_NOTAIMING;
    portHudGunSight(&gdl);
    assert(calls == 2 && !ringDrawn && drawnX == 200.5f && drawnY == 90.5f);
    assert(drawnScaleX == 0.25f && drawnScaleY == 0.5f);
    adsBlend = 1.0f;
    portHudGunSight(&gdl); assert(calls == 2); /* iron sights hide targets */
    portCrosshairAdsSet(1); /* ADS Crosshair (FORK-060): the always-on cross at the aim point */
    portHudGunSight(&gdl);
    assert(calls == 3 && !ringDrawn && drawnX == 200.5f && drawnY == 90.5f);
    player.bonddead = 1; portHudGunSight(&gdl); assert(calls == 3); player.bonddead = 0;
    portCrosshairAdsSet(0); calls = 2;
    scopeBlend = 0.5f;
    portHudGunSight(&gdl);
    assert(calls == 3 && !ringDrawn && drawnX == 360.0f && drawnY == 130.0f);
    assert(drawnColor == 0xff3a3237); /* scope colour and squared fade */
    player.mpmenuon = 1; portHudGunSight(&gdl); assert(calls == 3);
    player.mpmenuon = 0; player.bonddead = 1;
    portHudGunSight(&gdl); assert(calls == 3);

    /* Arc Impact Crosshair: the always-on cross at each arc's impact, through
     * the same wide-aim conversion; with none, at the aim point. */
    player.bonddead = 0; scopeBlend = 0.0f; adsBlend = 0.0f; reach = 2.0f;
    gdl = (s32)commands;   /* each wide draw adds its anchor commands */
    arcImpacts = 2;
    portHudGunSight(&gdl);
    assert(calls == 5 && !ringDrawn && drawnX == 140.0f && drawnY == 80.0f);
    arcImpacts = 1; dashedDrawn = 0;
    portHudGunSight(&gdl);
    assert(calls == 6 && drawnX == 100.0f && drawnY == 60.0f && !dashedDrawn);
    arcImpactAt[0][2] = 0.5f;                         /* out of sight: dashed, fainter */
    portHudGunSight(&gdl);
    assert(calls == 7 && drawnX == 100.0f && dashedDrawn && (drawnColor & 0xff) == 0xdc * 2 / 3);
    arcImpactAt[0][2] = -1.0f; calls = 6; gdl = (s32)commands;
    arcImpactAt[0][3] = 0.5f; hudScale = 0;            /* a far impact: half size */
    portHudGunSight(&gdl);
    assert(calls == 7 && drawnScaleY == 0.5f && drawnScaleX == 0.25f);
    arcImpactAt[0][3] = 1.0f; hudScale = 1; calls = 6; gdl = (s32)commands;
    adsBlend = 1.0f; portCrosshairAdsSet(1);          /* and with ADS Crosshair in ADS */
    portHudGunSight(&gdl);
    assert(calls == 7 && drawnX == 100.0f && drawnY == 60.0f);
    portCrosshairAdsSet(0); adsBlend = 0.0f;
    arcImpacts = 0;
    portHudGunSight(&gdl);
    assert(calls == 8 && drawnX == 200.5f && drawnY == 90.5f);
    reach = 1.0f; calls = 3;
}

static void crosshairSizingChecks(void)
{
    /* Arrange: always-on target with fractional aim position and a custom colour. */
    Gfx commands[32];
    s32 gdl = (s32)commands, i, high, wide;
    const s32 sizes[] = {10, 25, 50, 100, 200};
    player.mpmenuon = player.bonddead = 0;
    player.gunsightmode = GUNSIGHTREASON_NOTAIMING;
    player.crosshair_angle.x = 100.25f; player.crosshair_angle.y = 90.5f;
    adsBlend = scopeBlend = 0;
    for (i = 0; i < 5; i++) for (high = 0; high < 2; high++) for (wide = 0; wide < 2; wide++) {
        const f32 expected = ((f32)sizes[i] / 100.0f) * (high ? 0.5f : 1.0f);
        hudScale = high; reach = wide ? 2.0f : 1.0f;
        portCrosshairSizeSet(sizes[i]);
        /* Act/assert: the target stays put and composes with HUD and widescreen scaling. */
        portHudGunSight(&gdl);
        assert(drawnX == (wide ? 200.5f : 100.25f) && drawnY == 90.5f);
        assert(drawnScaleY == expected && drawnScaleX == expected * (wide ? 0.5f : 1));
        assert(drawnColor == 0x123456dc && !ringDrawn);
        portCrosshairSliderDraw(commands, 175, 27, 65, 304, 0x123456, high);
        assert(drawnScaleX == expected && drawnScaleY == expected && drawnX == 304);
        assert(drawnColor == 0x123456dc && !ringDrawn);
    }
    /* Other sights/cursors retain their original size, even at the minimum. */
    portCrosshairSizeSet(10); hudScale = 0; reach = 1;
    player.gunsightmode = 0;
    portHudGunSight(&gdl);
    assert(ringDrawn && drawnScaleY == 1);
    scopeBlend = 1;
    portHudGunSight(&gdl);
    assert(!ringDrawn && drawnScaleY == 1 && drawnColor == 0xff3a32dc);
    assert(portCrosshairSizeFromFraction(-1) == 10);
    assert(portCrosshairSizeFromFraction(1) == 200);
    assert(portCrosshairSizeFromFraction(0.5f) == 105);
    portCrosshairSizeSet(100);
}

int main(void)
{
    projectionChecks();
    sightChecks();
    crosshairSizingChecks();
    puts("Upstream merge checks passed: optional weapon FOV, calibrated ADS, watch projection, vector sights and HUD scale.");
    return 0;
}
