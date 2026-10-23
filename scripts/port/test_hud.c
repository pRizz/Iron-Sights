/* ROM-free presentation state checks; the renderer is discarded by gc-sections. */
#include <assert.h>
#include <stdarg.h>
#include "../../port/src/port_hud.c"
#include "../../port/src/port_wardrobe.c"
#include "port_icon_decode.h"

static struct player testPlayer;
struct player *g_CurrentPlayer = &testPlayer;
f32 g_GlobalTimerDelta;
static s32 testSeat, live = 1, playerCount = 1;
static s32 inventory[40], inventoryCount, currentWeapon;
static save_data bestSave;
static s32 bestTimes[3][3];
s32 selected_folder_num;
s32 fileGetBondForCurrentFolder(void) { return BOND_CONNERY; }
save_data *fileGetSaveForFoldernum(u32 folder) { return folder == 0 ? &bestSave : NULL; }
s32 fileGetSaveStageDifficultyTime(save_data *save, LEVEL_SOLO_SEQUENCE level, DIFFICULTY difficulty)
{ assert(save == &bestSave); return bestTimes[level][difficulty]; }
/* Deliberately shuffled IDs and header rows catch stage/index confusion. */
struct mission_folder_setup mission_folder_setup_entries[] = {
    {.folder_text_preset=1, .stage_id=LEVELID_NONE, .mission_num=-1},
    {.folder_text_preset=2, .stage_id=LEVELID_FACILITY, .mission_num=1},
    {.folder_text_preset=3, .stage_id=LEVELID_DAM, .mission_num=0},
    {.folder_text_preset=4, .stage_id=LEVELID_RUNWAY, .mission_num=2},
    {0}
};
s16 solo_target_time_array[20][3] = {{0,160,0}, {0,0,125}, {300,0,0}};
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
    {
        char timer[64], tiny[5];
        bestTimes[0][DIFFICULTY_SECRET] = 123;
        bestTimes[1][DIFFICULTY_AGENT] = 1023;
        assert(portHudMissionBestSeconds(LEVELID_DAM, DIFFICULTY_SECRET) == 123);
        assert(portHudMissionBestSeconds(LEVELID_FACILITY, DIFFICULTY_AGENT) == 1023);
        assert(portHudMissionBestSeconds(LEVELID_DAM, DIFFICULTY_AGENT) == 0);
        assert(portHudMissionBestSeconds(LEVELID_DAM, DIFFICULTY_007) == 0);
        assert(portHudMissionBestSeconds(LEVELID_TITLE, DIFFICULTY_AGENT) == 0);
        assert(portHudMissionBestSeconds(LEVELID_DAM, -1) == 0);
        selected_folder_num = 1;
        assert(portHudMissionBestSeconds(LEVELID_DAM, DIFFICULTY_SECRET) == 0);
        selected_folder_num = 0;
        assert(portHudMissionTargetSeconds(LEVELID_DAM, DIFFICULTY_SECRET) == 160);
        assert(portHudMissionTargetSeconds(LEVELID_FACILITY, DIFFICULTY_00) == 125);
        assert(portHudMissionTargetSeconds(LEVELID_RUNWAY, DIFFICULTY_AGENT) == 300);
        assert(portHudMissionTargetSeconds(LEVELID_DAM, DIFFICULTY_AGENT) == 0);
        assert(portHudMissionTargetSeconds(LEVELID_DAM, DIFFICULTY_007) == 0);
        assert(portHudMissionTargetSeconds(LEVELID_NONE, DIFFICULTY_AGENT) == 0);
        assert(portHudMissionTargetSeconds(LEVELID_TITLE, DIFFICULTY_SECRET) == 0);
        assert(portHudMissionTargetSeconds(LEVELID_DAM, -1) == 0);
        assert(portHudMissionTargetSeconds(LEVELID_DAM, 999) == 0);
        portHudMissionTimerText(42*60+30, 160, timer, sizeof(timer));
        assert(strcmp(timer, "00:42:50 / CHEAT 02:40") == 0);
        portHudMissionTimerText(60*60, 0, timer, sizeof(timer));
        assert(strcmp(timer, "01:00:00") == 0);
        portHudMissionTimerText(59, 0, timer, sizeof(timer));
        assert(strcmp(timer, "00:00:98") == 0);
        portHudMissionTimerText(-1, 0, timer, sizeof(timer));
        assert(strcmp(timer, "00:00:00") == 0);
        portHudMissionTimerText(100*3600, 160, timer, sizeof(timer));
        assert(strcmp(timer, "100:00:00 / CHEAT 02:40") == 0);
        portHudMissionTimerText(42*60, 160, tiny, sizeof(tiny));
        assert(tiny[sizeof(tiny)-1] == '\0');
        portHudMissionTimerText(0, 0, NULL, 0);
        assert(portHudMissionTargetColor(149*60+59, 160) == 0xb9ed9fff);
        assert(portHudMissionTargetColor(150*60, 160) == 0xf0d994ff);
        assert(portHudMissionTargetColor(160*60+59, 160) == 0xf0d994ff);
        assert(portHudMissionTargetColor(161*60, 160) == 0xff9d91ff);
        /* Restoring the mission clock immediately restores the comparison. */
        assert(portHudMissionTargetColor(42*60, 160) == 0xb9ed9fff);
        assert(portHudMissionTargetColor(999999, 0) == 0xffffffff);
    }
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

    /* Idle presentation uses simulation time, pauses, and stays per player. */
    assert(portHudOptionGet("always_health", &value) && value == 1);
    for (i = 0; i < 4; i++) {
        portHudReset(0);
        currentWeapon = ITEM_AK47;
        testPlayer.bondhealth = 0.75f; testPlayer.bondarmour = 0.5f;
        g_GlobalTimerDelta = 60.0f/rates[i];
        portHudTick(); /* seed the initial state */
        for (j = 0; j < rates[i]*3; j++) portHudTick();
        assert(phQuietScale(phState[0].weaponIdle, 0.2f) == 1);
        assert(phQuietScale(phState[0].weaponIdle, 0) == 1); /* selected caption */
        assert(phQuietScale(phState[0].healthIdle, 0.25f) == 1);
        for (j = 0; j < rates[i]; j++) portHudTick();
        assert(phState[0].weaponIdle == 240 && phState[0].healthIdle == 240);
        assert(phQuietScale(phState[0].weaponIdle, 0) == 0);
        assert(phQuietScale(210, 0.2f) > 0.59f && phQuietScale(210, 0.2f) < 0.61f);
        currentWeapon = ITEM_WPPK; portHudTick();
        assert(phState[0].weaponIdle == 0 && phState[0].healthIdle == 240);
        assert(phQuietScale(phState[0].weaponIdle, 0) == 1);
        testPlayer.bondarmour = 0.75f; portHudTick();
        assert(phState[0].healthIdle == 0 && phState[0].weaponIdle > 0);
        phState[0].weaponIdle = phState[0].healthIdle = 210;
        live = 0; portHudTick(); live = 1;
        assert(phState[0].weaponIdle == 210 && phState[0].healthIdle == 210);
        portHudReset(1); testSeat = 1; portHudTick(); testSeat = 0;
        assert(phState[1].weaponIdle == 0 && phState[0].weaponIdle == 210);
    }
    portHudOptionSet("damage_indicators", 0);
    portHudDamage(0, 1, 0);
    assert(phState[0].healthIdle == 0 && phState[0].damage[0].time == 0);
    portHudOptionSet("damage_indicators", 1);
    portHudOptionSet("weapon_hud", 0);
    assert(portHudAlwaysHealthShown()); /* independent of the weapon strip */
    portHudOptionSet("weapon_hud", 1);
    testPlayer.bonddead = 1; assert(!portHudAlwaysHealthShown()); testPlayer.bonddead = 0;
    testPlayer.mpmenuon = 1; assert(!portHudAlwaysHealthShown()); testPlayer.mpmenuon = 0;
    testPlayer.hudmessoff = 1; assert(!portHudAlwaysHealthShown()); testPlayer.hudmessoff = 0;
    {
        struct damage_display_val gauge[2] = {0};
        gauge[0].pos.x = 624; gauge[1].pos.x = 520;
        gauge[0].colour.a = 255; gauge[1].colour.a = 48;
        phState[0].healthIdle = 0;
        portHudStyleGauge(gauge, 2);
        assert(gauge[0].pos.x == 611 && gauge[1].pos.x == 533 && gauge[0].colour.a == 255);
        /* Rendering regenerates the native geometry before applying the style. */
        gauge[0].pos.x = 624; gauge[1].pos.x = 520;
        phState[0].healthIdle = 240;
        portHudStyleGauge(gauge, 2);
        assert(gauge[0].pos.x == 582 && gauge[1].pos.x == 561);
        assert(gauge[0].colour.a == 64 && gauge[1].colour.a == 12);
        portHudOptionSet("always_health", 0);
        assert(!portHudAlwaysHealthShown());
        portHudStyleGauge(gauge, 2);
        assert(gauge[0].pos.x == 582 && gauge[0].colour.a == 64);
        portHudOptionSet("always_health", 1);
    }
    assert(phOpacity(0xabcdef80, 1) == 0xabcdef80);
    assert(phOpacity(0xabcdef80, 0.2f) == 0xabcdef1a);
    assert(phOpacity(0xabcdef80, 0) == 0xabcdef00);

    inventory[0] = ITEM_UNARMED; inventory[1] = ITEM_WPPK; inventory[2] = ITEM_AK47;
    inventory[3] = ITEM_WPPK; inventory[4] = ITEM_DOORDECODER; inventory[5] = ITEM_SNIPERRIFLE;
    inventory[6] = ITEM_REMOTEMINE; inventory[7] = ITEM_TRIGGER;
    inventoryCount = 8; currentWeapon = ITEM_AK47;
    n = phWeapons(weapons, &selected);
    assert(n == 5 && selected == 2 && weapons[4].item == ITEM_REMOTEMINE);
#if PORT_WEAPON_IMAGES_AVAILABLE
    for (i = ITEM_UNARMED; i <= ITEM_TASER; i++) {
        if (i == ITEM_WATCHLASER || i == ITEM_TRIGGER) continue;
        assert(phWeaponIcon(i) >= 0 && phWeaponIcon(i) < PORT_WEAPON_IMAGE_COUNT);
    }
    assert(phWeaponIcon(ITEM_DOORDECODER) == -1);
    assert(phWeaponIcon(ITEM_WPPK) != phWeaponIcon(ITEM_WPPKSIL));
    assert(phWeaponIcon(ITEM_MP5K) != phWeaponIcon(ITEM_MP5KSIL));
    assert(phWeaponIcon(ITEM_TIMEDMINE) != phWeaponIcon(ITEM_REMOTEMINE));
    if (PORT_WEAPON_IMAGE_COUNT >= 38) {
        for (i = 0; i < 6; i++) {
            testPlayer.bondtype = portWardrobeCuff(i);
            assert(phWeaponIcon(ITEM_FIST) == 32+i && phWeaponIcon(ITEM_UNARMED) == 32+i);
        }
        testPlayer.bondtype = CUFF_FOLDER;
        assert(phWeaponIcon(ITEM_FIST) == 37);
        testPlayer.bondtype = CUFF_BLUE;
    }
    {
        FILE *file = fopen("assets/port/weapon_icons/weapon_icons.rle", "rb");
        const s32 bytes = PORT_WEAPON_IMAGE_WIDTH*PORT_WEAPON_IMAGE_HEIGHT*4;
        u8 *packed = malloc(PORT_WEAPON_IMAGE_BYTES), *pixels = malloc(bytes+1);
        const u8 badCount[] = {0,0}, truncated[] = {1,128,255}, overflow[] = {2,128,0,0,0,255};
        assert(file && packed && pixels);
        assert(fread(packed, 1, PORT_WEAPON_IMAGE_BYTES, file) == PORT_WEAPON_IMAGE_BYTES);
        fclose(file);
        for (i = 0; i < PORT_WEAPON_IMAGE_COUNT; i++) if (portWeaponImages[i].size) {
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
    puts("HUD checks passed: settings, idle fading/gauge geometry, icon coverage/decoding, per-player feedback, direction, queued objectives, inventory filtering/scrolling, 30/60/120/240 Hz.");
    return 0;
}
