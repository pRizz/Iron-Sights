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
static s32 weaponFov = 1, hudScale, calls, ringDrawn;
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
Gfx *portHudPanel(Gfx *g, f32 x, f32 y, f32 w, f32 h, u32 color) { return g; }
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
    scopeBlend = 0.5f;
    portHudGunSight(&gdl);
    assert(calls == 3 && !ringDrawn && drawnX == 360.0f && drawnY == 130.0f);
    assert(drawnColor == 0xff3a3237); /* scope colour and squared fade */
    player.mpmenuon = 1; portHudGunSight(&gdl); assert(calls == 3);
    player.mpmenuon = 0; player.bonddead = 1;
    portHudGunSight(&gdl); assert(calls == 3);
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
