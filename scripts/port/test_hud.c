/* ROM-free presentation state checks; the renderer is discarded by gc-sections. */
#include <assert.h>
#include <stdarg.h>
#include "../../port/src/port_hud.c"
#include "port_icon_decode.h"

static struct player testPlayer;
struct player *g_CurrentPlayer = &testPlayer;
f32 g_GlobalTimerDelta;
static s32 testSeat, live = 1, playerCount = 1;
static s32 inventory[40], inventoryCount, currentWeapon;
s32 get_cur_playernum(void) { return testSeat; }
s32 getPlayerCount(void) { return playerCount; }
s32 portBondControlsLive(void) { return live; }
s32 bondinvCountTotalItemsInInv(void) { return inventoryCount; }
s32 bondinvGetTextbyInvIndex(s32 i) { return inventory[i]; }
ITEM_IDS getCurrentPlayerWeaponId(GUNHAND hand) { (void)hand; return currentWeapon; }
void osSyncPrintf(const char *fmt, ...)
{
    va_list args; va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args); abort();
}

int main(void)
{
    s32 i, j, value, selected, n;
    const s32 rates[] = {30,60,120,240};
    HudWeapon weapons[HUD_WEAPONS];
    for (i = 0; i < 4; i++) assert(portHudOptionGet(phKeys[i], &value) && value == 1);
    assert(portHudOptionGet("weapon_hud_icons", &value) && value == 0);
    assert(portHudOptionSet("weapon_hud_icons", 1));
    assert(portHudOptionGet("weapon_hud_icons", &value) && value == 1);
    portHudReset(0); /* changing presentation does not depend on stage state */
    assert(portHudOptionGet("weapon_hud_icons", &value) && value == 1);
    assert(portHudOptionSet("weapon_hud_icons", 0));
    assert(!portHudOptionGet("not_a_setting", &value));
    assert(!portHudOptionSet("not_a_setting", 0));
    portHudHit(-1, 1); portHudDamage(4, 1, 0);
    assert(phState[0].hitTime == 0);
    portHudHit(1, 1);
    assert(phState[1].hitTime > 0 && phState[0].hitTime == 0);
    portHudHit(1, 0);
    assert(phState[1].headshot); /* multiple pellets retain the headshot cue */
    portHudDamage(0, 0, 1); /* incoming from behind, facing north */
    assert(cosf(phDamageAngle(&phState[0].damage[0], 0)) < -0.99f);
    assert(sinf(phDamageAngle(&phState[0].damage[0], 90)) > 0.99f);
    portHudDamage(0, 0, 0);
    assert(!phState[0].damage[1].directional);
    for (i = 0; i < 12; i++) portHudDamage(0, 1, 0);
    assert(phState[0].damage[3].time == 60);

    { /* Use the engine's forward/right movement basis as an independent
       * reference, rather than assuming world +X is screen-right. */
        const f32 headings[] = {0, 37, 90, 180, 270, 315};
        const f32 sources[][2] = {{0,1}, {1,0}, {0,-1}, {-1,0}, {1,1}, {-1,-1}};
        for (i = 0; i < 6; i++) for (j = 0; j < 6; j++) {
            const f32 h = headings[i] * HUD_PI / 180;
            const f32 right = sources[j][0], forward = sources[j][1];
            const f32 sourceX = -cosf(h)*right - sinf(h)*forward;
            const f32 sourceZ = -sinf(h)*right + cosf(h)*forward;
            const f32 length = sqrtf(right*right + forward*forward);
            f32 angle;
            portHudReset(0);
            portHudDamage(0, -sourceX, -sourceZ);
            angle = phDamageAngle(&phState[0].damage[0], headings[i]);
            {
                const f32 dx = sinf(angle) - right/length;
                const f32 dy = -cosf(angle) + forward/length;
                assert(dx > -0.0001f && dx < 0.0001f);
                assert(dy > -0.0001f && dy < 0.0001f);
            }
        }
        portHudReset(0);
        portHudDamage(0, 1, 0); /* source on right at heading 0 */
        assert(sinf(phDamageAngle(&phState[0].damage[0], 0)) > 0.99f);
        assert(cosf(phDamageAngle(&phState[0].damage[0], 90)) > 0.99f); /* turn toward it */
        assert(sinf(phDamageAngle(&phState[0].damage[0], 180)) < -0.99f); /* now on left */
    }

    for (i = 0; i < 4; i++) {
        portHudReset(0);
        portHudHit(0, 0); portHudDamage(0, -1, 0);
        assert(portHudObjective(0, 0, OBJECTIVESTATUS_COMPLETE, "Neutralise all alarms\n"));
        assert(portHudObjective(0, 0, OBJECTIVESTATUS_COMPLETE, "duplicate"));
        assert(phState[0].toastCount == 1);
        assert(strcmp(phState[0].toasts[0].text, "Neutralise all alarms") == 0);
        portHudObjective(1, 1, OBJECTIVESTATUS_FAILED, "Protect the scientist");
        g_GlobalTimerDelta = 60.0f / rates[i];
        for (j = 0; j < rates[i] / 4; j++) portHudTick();
        assert(phState[0].hitTime == 0 && phState[0].damage[0].time > 0);
        live = 0;
        { const f32 time = phState[0].toastTime; portHudTick(); assert(phState[0].toastTime == time); }
        live = 1;
        for (j = 0; j < rates[i]*5; j++) portHudTick();
        assert(phState[0].damage[0].time == 0);
        assert(phState[0].toastCount == 1 && phState[0].toasts[0].index == 1);
        for (j = 0; j < rates[i]*5; j++) portHudTick();
        assert(phState[0].toastCount == 0);
    }
    for (i = 0; i < 20; i++) portHudObjective(i, i%10, OBJECTIVESTATUS_COMPLETE, "queued");
    assert(phState[0].toastCount == HUD_TOASTS && phState[0].toasts[0].index == 0);
    assert(phState[0].toasts[HUD_TOASTS-1].index == 19);
    playerCount = 2;
    assert(!portHudObjective(0,0,OBJECTIVESTATUS_FAILED,"MP"));
    playerCount = 1;
    portHudOptionSet("objective_notifications", 0);
    assert(!phState[0].toastCount && !portHudObjective(0,0,0,"off"));
    portHudOptionSet("hit_markers", 0); portHudHit(1, 1);
    assert(phState[1].hitTime == 0);
    portHudOptionSet("damage_indicators", 0); portHudDamage(0, 0, 1);
    assert(phState[0].damage[0].time == 0);
    portHudOptionSet("weapon_hud", 0); assert(!portHudWeaponStripShown());
    for (i = 0; i < 4; i++) portHudOptionSet(phKeys[i], 1);
    assert(portHudWeaponStripShown());
    live = 0; assert(!portHudWeaponStripShown()); live = 1;

    inventory[0] = ITEM_UNARMED; inventory[1] = ITEM_WPPK; inventory[2] = ITEM_AK47;
    inventory[3] = ITEM_WPPK; inventory[4] = ITEM_DOORDECODER; inventory[5] = ITEM_SNIPERRIFLE;
    inventory[6] = ITEM_REMOTEMINE; inventory[7] = ITEM_TRIGGER;
    inventoryCount = 8; currentWeapon = ITEM_AK47;
    n = phWeapons(weapons, &selected);
    assert(n == 5 && selected == 2 && weapons[4].item == ITEM_REMOTEMINE);
#if PORT_WEAPON_IMAGES_AVAILABLE
    for (i = ITEM_UNARMED; i <= ITEM_TASER; i++) {
        if (i == ITEM_WATCHLASER || i == ITEM_TRIGGER) continue;
        assert(phWeaponIcon(i) >= 0 && phWeaponIcon(i) < 32);
    }
    assert(phWeaponIcon(ITEM_DOORDECODER) == -1);
    assert(phWeaponIcon(ITEM_WPPK) != phWeaponIcon(ITEM_WPPKSIL));
    assert(phWeaponIcon(ITEM_MP5K) != phWeaponIcon(ITEM_MP5KSIL));
    assert(phWeaponIcon(ITEM_TIMEDMINE) != phWeaponIcon(ITEM_REMOTEMINE));
    {
        FILE *file = fopen("assets/port/weapon_icons/weapon_icons.rle", "rb");
        const s32 bytes = PORT_WEAPON_IMAGE_WIDTH*PORT_WEAPON_IMAGE_HEIGHT*4;
        u8 *packed = malloc(PORT_WEAPON_IMAGE_BYTES), *pixels = malloc(bytes+1);
        const u8 badCount[] = {0,0}, truncated[] = {1,128,255}, overflow[] = {2,128,0,0,0,255};
        assert(file && packed && pixels);
        assert(fread(packed, 1, PORT_WEAPON_IMAGE_BYTES, file) == PORT_WEAPON_IMAGE_BYTES);
        fclose(file);
        for (i = 0; i < 32; i++) if (portWeaponImages[i].size) {
            const PortWeaponImage *image = &portWeaponImages[i];
            s32 visible = 0;
            assert(image->offset <= PORT_WEAPON_IMAGE_BYTES && image->size <= PORT_WEAPON_IMAGE_BYTES-image->offset);
            pixels[bytes] = 0x5a;
            assert(portIconDecode(packed+image->offset, image->size, pixels, bytes));
            assert(pixels[bytes] == 0x5a);
            for (j = 3; j < bytes; j += 4) visible += pixels[j] > 0;
            assert(visible > 100);
        }
        assert(!portIconDecode(badCount, sizeof(badCount), pixels, bytes));
        assert(!portIconDecode(truncated, sizeof(truncated), pixels, bytes));
        assert(!portIconDecode(overflow, sizeof(overflow), pixels, 4));
        free(packed); free(pixels);
    }
#else
    assert(phWeaponIcon(ITEM_AK47) == -1); /* exporter bootstrap falls back to names */
#endif
    currentWeapon = ITEM_DOORDECODER;
    phWeapons(weapons, &selected); assert(selected == -1);
    for (i = 0; i < 32; i++) for (j = 1; j <= 5; j++) {
        const s32 start = phStripStart(32, i, j);
        assert(start >= 0 && start + j <= 32 && start <= i && start + j > i);
    }
    portHudReset(1); assert(phState[1].hitTime == 0);
    assert(portHudOptionGet("hit_markers", &value) && value == 1);
    puts("HUD checks passed: settings, real-model icon coverage/decoding, per-player feedback, direction, queued objectives, inventory filtering/scrolling, 30/60/120/240 Hz.");
    return 0;
}
