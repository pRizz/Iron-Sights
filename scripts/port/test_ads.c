/* ROM-free ADS input/state checks. Run with scripts/port/test_ads.sh. */
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
extern void *memset(void *dst, int value, size_t size);
#include "../../port/src/joy_port.c"
#include "../../port/src/port_knife_ads.c"
#include "../../port/src/port_wardrobe.c"
#include "../../port/src/ads_hands.c"
#include "../../port/src/port_options.c"
#include "../../port/src/port_hud.c"
#include "../../port/src/port_crosshair.c"

static s32 iconFixtureReady = 1;
s32 portIconCacheReady(void) { return iconFixtureReady; }
s32 portIconAvailable(s32 icon) { return iconFixtureReady && icon >= 0 && icon < 38 && icon != 23 && icon != 30; }
static struct player players[4];
struct player *g_CurrentPlayer;
s32 g_PlayerIsInTank;
f32 g_GlobalTimerDelta;
static s32 currentPlayer;
static s32 controlsLive;
static OSTime testTime = 1;
static s32 savedSprint = -1;
static s32 savedKnifeStyle = -1;
static s32 savedCrosshairSize = -1;
static s32 savedHud[6] = {-1,-1,-1,-1,-1,-1};
u32 osVirtualToPhysical(void *address) { return (u32)address; }

s32 portVideoOptionGet(const char *key, s32 *out) { (void) key; (void) out; return 0; }
s32 portVideoOptionSet(const char *key, s32 v) { (void) key; (void) v; return 0; }
s32 portInputOptionGet(const char *key, s32 *out) { (void) key; (void) out; return 0; }
s32 portInputOptionSet(const char *key, s32 v) { (void) key; (void) v; return 0; }
s32 portIniWriteStrs(const char *section, const char *const *keys, const char *const *vals, s32 n)
{
    s32 i;
    for (i = 0; i < n; i++) {
        if (strcmp(keys[i], "knife_ads") == 0) savedKnifeStyle = atoi(vals[i]);
        if (strcmp(keys[i], "always_crosshair_size") == 0) savedCrosshairSize = atoi(vals[i]);
        s32 j;
        for (j = 0; j < HUD_OPTIONS; j++) if (strcmp(keys[i], phKeys[j]) == 0) savedHud[j] = atoi(vals[i]);
        if (strcmp(keys[i], "sprint") == 0) {
            assert(strcmp(section, "[display]") == 0);
            savedSprint = atoi(vals[i]);
        }
    }
    return 1;
}

s32 get_cur_playernum(void) { return currentPlayer; }
s32 portBondControlsLive(void) { return controlsLive; }
ITEM_IDS getCurrentPlayerWeaponId(GUNHAND hand) { return g_CurrentPlayer->hand_item[hand]; }
s32 get_itemtype_in_hand(GUNHAND hand) { return g_CurrentPlayer->hand_item[hand]; }
OSTime osGetTime(void) { return testTime; }
s32 portAdsP1SniperActive(void)
{
    struct player *saved = g_CurrentPlayer;
    s32 savedNum = currentPlayer;
    s32 active;
    currentPlayer = 0;
    g_CurrentPlayer = &players[0];
    active = portAdsActive() && getCurrentPlayerWeaponId(GUNRIGHT) == ITEM_SNIPERRIFLE;
    currentPlayer = savedNum;
    g_CurrentPlayer = saved;
    return active;
}
void portInputRecActions(u8 *down, s32 count) { (void) down; (void) count; }
void portConLogf(const char *fmt, ...) { (void) fmt; }
s32 portProfileIniKey(const char *key, const char *value) { (void) key; (void) value; return 0; }
/* The game's assert macro logs through osSyncPrintf; make failed checks fatal. */
void osSyncPrintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    abort();
}

static void selectPlayer(s32 player)
{
    currentPlayer = player;
    g_CurrentPlayer = &players[player];
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_WPPKSIL;
    g_CurrentPlayer->hand_item[GUNLEFT] = ITEM_UNARMED;
    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_IDLE;
}

static void pollAds(s32 pad, s32 held)
{
    if (held) portJoyPortPadDown(pad, PORTACT_AIM_DOWN_SIGHTS);
    portJoyPortPollCommit();
}

static void normalAim(s32 held, s32 pressed, s32 toggle)
{
    g_CurrentPlayer->insightaimmode = portAdsUpdateAim(held, pressed, toggle);
}

static void testSprint(void)
{
    s32 setting;
    const s32 opt = portOptFind("sprint");
    assert(opt >= 0 && portOptAvailable(opt) && portOptIsToggle(opt));
    assert(portOptCategoryOf(opt) == PORTOPT_CAT_GAMEPLAY);
    assert(portJoyPortOptionGet("sprint", &setting) && setting == 1);
    assert(strcmp(portJoyPortBindDefault(PORTACT_WALK), "Left Shift") == 0);
    assert(portJoyPortMoveScale(0) == 1.0f);
    portJoyPortPadDown(0, PORTACT_WALK);
    portJoyPortPollCommit();
    assert(portJoyPortMoveScale(0) == 1.5f);
    assert(portJoyPortMoveScale(1) == 1.0f); /* native crouch slowdown still applies */
    g_CurrentPlayer->insightaimmode = 1;
    assert(portJoyPortMoveScale(0) == 1.0f);
    g_CurrentPlayer->insightaimmode = 0;
    normalAim(1, 1, 0);
    assert(portJoyPortMoveScale(0) == 0.5f); /* ADS wins over held sprint */
    assert(portJoyPortMoveScale(1) == 0.5f);
    normalAim(0, 0, 0);

    selectPlayer(1);
    assert(portJoyPortMoveScale(0) == 1.0f); /* player 1's key cannot sprint player 2 */
    portJoyPortPadDown(1, PORTACT_WALK);
    portJoyPortPollCommit();
    assert(portJoyPortMoveScale(0) == 1.5f);
    selectPlayer(0);
    assert(portJoyPortMoveScale(0) == 1.0f);
    portJoyPortPadDown(0, PORTACT_WALK);
    portJoyPortPollCommit();
    controlsLive = 0;
    assert(portJoyPortMoveScale(0) == 1.0f);
    controlsLive = 1;
    g_PlayerIsInTank = 1;
    assert(portJoyPortMoveScale(0) == 1.0f);
    g_PlayerIsInTank = 0;
    currentPlayer = -1;
    assert(portJoyPortMoveScale(0) == 1.0f);
    selectPlayer(0);

    assert(portJoyPortSetOption("sprint", "0")); /* saved ini uses the string bridge */
    assert(portJoyPortOptionGet("sprint", &setting) && setting == 0);
    assert(portJoyPortMoveScale(0) == 0.5f);
    assert(portJoyPortMoveScale(1) == 0.5f);
    normalAim(1, 1, 0);
    assert(portJoyPortMoveScale(0) == 0.5f); /* walk and ADS slowdowns do not stack */
    normalAim(0, 0, 0);
    portJoyPortPollCommit();
    assert(portJoyPortMoveScale(0) == 1.0f);
    assert(portJoyPortOptionSet("sprint", 1)); /* menus use the integer bridge */
    assert(portJoyPortOptionGet("sprint", &setting) && setting == 1);
    portOptStep(opt, 1);
    assert(portOptGet(opt) == 0 && savedSprint == 0);
    portOptStep(opt, -1);
    assert(portOptGet(opt) == 1 && savedSprint == 1);
}

#include "test_knife_ads.c"
#include "test_rocket_ads.c"

int main(void)
{
    s32 i;
    struct MoveData move;
    s32 mode;
    coord3d muzzle = { 0.0f, 23.273382f, 705.7365f };
    coord3d pose;
    coord3d trigger, support;
    Mtxf scene = { .m = { {1.5f,0,0,0}, {0,2,0,0}, {0,0,-1.2f,-1}, {0,0,-2.2f,0} } };
    Mtxf handProjection;
    f32 previousDepth = -1.0f;
    const s32 rates[] = { 30, 60, 120, 240 };

    controlsLive = 1;
    selectPlayer(0);
    testKnifeAds();
    testRocketAds();
    testSprint();
    { /* The support palm faces under/into the gun. The watch follows that
       * roll rather than being billboarded towards the camera. */
        Mtxf gun = {.m={{-1,0,0,0},{0,1,0,0},{0,0,-1,0},{0,-4,-42,1}}};
        s32 grip, phase, left, k;
        for (grip = 0; grip < sizeof(sAdsGrips)/sizeof(sAdsGrips[0]); grip++) {
            for (phase = 0; phase <= 10; phase++) for (left = 0; left < 2; left++) {
                AdsArmPose arm = adsNativePose(&gun,left ? sAdsGrips[grip].support : sAdsGrips[grip].trigger,left,phase/10.0f);
                f32 backLength=0, sideLength=0, dot=0;
                for (k = 0; k < 3; k++) {
                    backLength += arm.back.f[k]*arm.back.f[k];
                    sideLength += arm.side.f[k]*arm.side.f[k];
                    dot += arm.back.f[k]*arm.axis.f[k];
                }
                assert(backLength > 0.999f && backLength < 1.001f);
                assert(sideLength > 0.999f && sideLength < 1.001f);
                assert(dot > -0.001f && dot < 0.001f);
                if (left) { assert(arm.back.x < 0 && arm.back.y < 0 && arm.back.z < 0); }
                else { assert(arm.back.x > 0); }
                { /* Closed-hand geometry stays rigid; its exact wrist points
                   * use the same transform as the attached forearm/watch. */
                    PortGripSpace space={{-6,-25,-67},{-3,-54,-182}};
                    coord3d a={-10,2,10},b={10,5,30},pa,pb,join,elbow;
                    f32 nativeLength=0,posedLength=0;
                    adsGripPoint(&a,&pa,PORT_ARM_HAND,&space,&arm);
                    adsGripPoint(&b,&pb,PORT_ARM_HAND,&space,&arm);
                    for (s32 d=0;d<3;d++) {
                        nativeLength+=(a.f[d]-b.f[d])*(a.f[d]-b.f[d]);
                        posedLength+=(pa.f[d]-pb.f[d])*(pa.f[d]-pb.f[d]);
                    }
                    assert(posedLength/nativeLength>.691f && posedLength/nativeLength<.694f); /* (.052*16)^2 */
                    /* Enlarging the rigid grip keeps the palm origin anchored
                     * to the weapon instead of shifting its contact point. */
                    a=(coord3d){0,0,0};
                    adsGripPoint(&a,&pa,PORT_ARM_HAND,&space,&arm);
                    for (s32 d=0;d<3;d++) assert(fabsf(pa.f[d]-16*arm.palm.f[d])<.001f);
                    adsGripPoint(&space.wrist,&pa,PORT_ARM_HAND,&space,&arm);
                    adsGripPoint(&space.wrist,&join,PORT_ARM_FOREARM,&space,&arm);
                    assert(!memcmp(&pa,&join,sizeof(pa)));
                    adsGripPoint(&space.elbow,&elbow,PORT_ARM_FOREARM,&space,&arm);
                    for (s32 d=0;d<3;d++) assert(elbow.f[d]-16*arm.elbow.f[d]>.001f*-1 && elbow.f[d]-16*arm.elbow.f[d]<.001f);
                    /* Bulk increases the arm body, leaving its center and
                     * the rigid hand/wrist transform untouched. */
                    a=space.elbow; a.x+=10;
                    adsGripPoint(&a,&pa,PORT_ARM_FOREARM,&space,&arm);
                    posedLength=0;
                    for (s32 d=0;d<3;d++) posedLength+=(pa.f[d]-elbow.f[d])*(pa.f[d]-elbow.f[d]);
                    assert(posedLength>432.62f && posedLength<432.66f); /* (10*.052*16*2.5)^2 */
                    /* Forearm shaping must never change a hand or watch,
                     * even for points outside the protected wrist band. */
                    adsGripPoint(&a,&pa,PORT_ARM_HAND,&space,&arm);
                    adsGripPoint(&a,&pb,PORT_ARM_WATCH,&space,&arm);
                    assert(!memcmp(&pa,&pb,sizeof(pa)));
                    a=space.wrist; a.z-=40;
                    adsGripPoint(&a,&pa,PORT_ARM_HAND,&space,&arm);
                    adsGripPoint(&a,&pb,PORT_ARM_FOREARM,&space,&arm);
                    assert(!memcmp(&pa,&pb,sizeof(pa)));
                    a.z-=.0001f;
                    adsGripPoint(&a,&pb,PORT_ARM_FOREARM,&space,&arm);
                    for (s32 d=0;d<3;d++) assert(pa.f[d]-pb.f[d]>-.02f && pa.f[d]-pb.f[d]<.02f);
                }
            }
        }
    }
    for (i = 0; i < 4; i++) {
        const s32 opt = portOptFind(phKeys[i]);
        assert(opt >= 0 && portOptAvailable(opt) && portOptGet(opt) == 1);
        portOptStep(opt, 1);
        assert(portOptGet(opt) == 0 && savedHud[i] == 0);
        assert(portJoyPortSetOption(phKeys[i], "1") && portOptGet(opt) == 1);
    }
    /* Arrange: absent old INI keys retain the original size, independently of visibility. */
    {
        const s32 opt = portOptFind("always_crosshair_size");
        assert(opt >= 0 && portOptAvailable(opt) && portOptIsRanged(opt));
        assert(portOptGet(opt) == 100 && portCrosshairSizeScale(0) == 1.0f);
        /* Act/assert: menu stepping saves, and the actual INI boundary reloads/clamps. */
        portOptStep(opt, -1);
        assert(portOptGet(opt) == 95 && savedCrosshairSize == 95);
        assert(portJoyPortSetOption("always_crosshair_size", "25"));
        assert(portOptGet(opt) == 25 && portCrosshairSizeScale(1) == 0.125f);
        assert(portJoyPortSetOption("always_crosshair_size", "-9") && portOptGet(opt) == 10);
        assert(portJoyPortSetOption("always_crosshair_size", "999") && portOptGet(opt) == 200);
        /* Pointer movement outside the track and an unheld pointer never change it. */
        assert(!portCrosshairSliderPick(opt, 100, 20, 0, 20, 52, 1));
        assert(!portCrosshairSliderPick(opt, 0, 20, 0, 20, 52, 0));
        assert(portCrosshairSliderPick(opt, 0, 20, 0, 20, 52, 1));
        assert(portOptGet(opt) == 10 && savedCrosshairSize == 10);
        assert(portCrosshairSliderPick(opt, 52, 20, 0, 20, 52, 1));
        assert(portOptGet(opt) == 200 && savedCrosshairSize == 200);
        portOptStep(opt, 1);
        assert(portOptGet(opt) == 200);
        portOptSetValue(opt, 10);
        portOptStep(opt, -1);
        assert(portOptGet(opt) == 10); /* slider endpoints never wrap */
        portOptSetValue(opt, 100);
        assert(savedCrosshairSize == 100);
    }
    assert(!portAdsActive());
    {
        const s32 opt = portOptFind("weapon_hud_icons");
        assert(opt >= 0 && portOptAvailable(opt) && !portOptIsToggle(opt));
        assert(portOptGet(opt) == 0);
        portOptStep(opt, 1);
        assert(portOptGet(opt) == 1 && savedHud[4] == 1);
        assert(portJoyPortSetOption("weapon_hud_icons", "0") && portOptGet(opt) == 0);
    }
    {
        const s32 opt = portOptFind("always_health");
        assert(opt >= 0 && portOptAvailable(opt) && !portOptIsToggle(opt));
        assert(portOptGet(opt) == 1);
        portOptStep(opt, 1);
        assert(portOptGet(opt) == 0 && savedHud[5] == 0);
        assert(portJoyPortSetOption("always_health", "1") && portOptGet(opt) == 1);
    }
    assert(PORTACT_CLASSIC_TOGGLE == 20);
    /* Appending upstream watch controls must not renumber saved fork bindings. */
    assert(PORTACT_AIM_DOWN_SIGHTS == 21 && PORTACT_STATE_MENU == 22);
    assert(PORTACT_QUICKSAVE == 23 && PORTACT_QUICKLOAD == 24 && PORTACT_UNDO_LOAD == 25);
    assert(PORTACT_WATCH_UP == 26 && PORTACT_WATCH_DOWN == 27 && PORTACT_COUNT == 28);
    assert(strcmp(portJoyPortBindDefault(PORTACT_WATCH_UP), "Up") == 0);
    portJoyPortPadDown(0, PORTACT_WATCH_DOWN);
    portJoyPortPollCommit();
    assert(portJoyPortWatchStepUpDown() == 1);
    assert(portJoyPortWatchStepUpDown() == 0);
    portJoyPortPollCommit();
    assert(strcmp(portJoyPortBindName(PORTACT_AIM_DOWN_SIGHTS), "aim_down_sights") == 0);
    assert(strcmp(portJoyPortBindDefault(PORTACT_AIM_DOWN_SIGHTS), "") == 0);

    adsHandProjection(&scene, &handProjection);
    for (i = 0; i < 4; i++) {
        assert(handProjection.m[i][0] == scene.m[i][0]);
        assert(handProjection.m[i][1] == scene.m[i][1]);
        assert(handProjection.m[i][3] == scene.m[i][3]);
    }
    for (i = 3; i <= 10; i++) {
        const f32 z = -(f32)i;
        const f32 clipZ = z*handProjection.m[2][2] + handProjection.m[3][2];
        const f32 clipW = z*handProjection.m[2][3] + handProjection.m[3][3];
        const f32 depth = (clipZ/clipW+1.0f)*0.5f;
        assert(depth > previousDepth && depth > 0.0f && depth < 0.01f);
        previousDepth = depth;
    }

    /* All fourteen firearms without original hands get grips; pistols and other items do not. */
    for (i = ITEM_UNARMED; i <= ITEM_CAMERA; i++)
    {
        const s32 hands = (i >= ITEM_SKORPION && i <= ITEM_SNIPERRIFLE)
            || i == ITEM_LASER || i == ITEM_GRENADELAUNCH || i == ITEM_ROCKETLAUNCH;
        assert(!!portAdsHandGrips(i, &trigger, &support) == hands);
        if (hands) {
            assert(support.z > trigger.z && trigger.x < 0.0f && support.x > 0.0f);
        }
    }

    assert(portAdsWeaponPose(ITEM_AK47, &muzzle, &pose));
    assert(pose.x == 0 && pose.y < -10.1f && pose.y > -10.2f && pose.z == -33.6f);
    assert(portAdsWeaponPitch(ITEM_AK47) < -0.04f && portAdsWeaponPitch(ITEM_AK47) > -0.05f);
    {
        /* Both D5Ks share the sight mesh. Their muzzle nodes differ by the
         * suppressor length, which must not shift the common sight vertically. */
        const coord3d shortMuzzle = {0.0f, 63.899998f, 338.669983f};
        const coord3d longMuzzle = {0.0f, 63.899998f, 538.786316f};
        coord3d shortPose, longPose;
        f32 dy;
        assert(portAdsWeaponPose(ITEM_MP5K, &shortMuzzle, &shortPose));
        assert(portAdsWeaponPose(ITEM_MP5KSIL, &longMuzzle, &longPose));
        assert(portAdsWeaponPitch(ITEM_MP5K) == portAdsWeaponPitch(ITEM_MP5KSIL));
        assert(shortPose.x == longPose.x && shortPose.z == longPose.z);
        dy = shortPose.y - longPose.y;
        assert(dy > -0.01f && dy < 0.01f);
    }
    muzzle.y = 57.956562f;
    assert(portAdsWeaponPose(ITEM_WPPK, &muzzle, &pose));
    assert(pose.y < -8.5f && pose.y > -8.7f && pose.z == -33.5f);
    /* The scoped rifle retains its own camera/scope transition distance. */
    assert(portAdsWeaponPose(ITEM_SNIPERRIFLE, &muzzle, &pose));
    assert(pose.z == -70.0f);
    for (i = ITEM_UNARMED; i <= ITEM_CAMERA; i++)
    {
        const s32 supported = (i >= ITEM_WPPK && i <= ITEM_LASER)
            || i == ITEM_GRENADELAUNCH || i == ITEM_ROCKETLAUNCH;
        assert(!!portAdsWeaponPose(i, &muzzle, &pose) == supported);
        if (supported) {
            assert(pose.z <= -28.0f && pose.y < -muzzle.y * 0.1f);
            assert(portAdsWeaponPitch(i) < 0.0f && portAdsWeaponPitch(i) >= -0.15f);
        } else {
            assert(portAdsWeaponPitch(i) == 0.0f);
        }
    }
    muzzle.y = 77.935936f;
    muzzle.z = 648.51721f;
    assert(portAdsWeaponPose(ITEM_AUTOSHOT, &muzzle, &pose));
    assert(portAdsWeaponPitch(ITEM_AUTOSHOT) < -0.1f);
    assert(pose.y < -18.5f && pose.y > -18.7f);

    assert(portJoyPortOptionGet("aim_mode", &mode) && mode == 1);
    normalAim(1, 1, 0);
    assert(portAdsActive() && !g_CurrentPlayer->insightaimmode);
    normalAim(0, 0, 0);
    assert(!portAdsActive());
    normalAim(1, 1, 1);
    normalAim(0, 0, 1);
    assert(portAdsActive());
    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_RELOAD_LOWER;
    normalAim(0, 0, 1);
    assert(!portAdsActive() && !g_CurrentPlayer->insightaimmode);
    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_IDLE;
    assert(portAdsActive());
    normalAim(1, 1, 1);
    assert(!portAdsActive());

    assert(portJoyPortSetOption("aim_mode", "0"));
    normalAim(1, 1, 0);
    assert(g_CurrentPlayer->insightaimmode && !portAdsActive());
    normalAim(0, 0, 0);
    assert(!g_CurrentPlayer->insightaimmode);
    normalAim(1, 1, 1);
    normalAim(0, 0, 1);
    assert(g_CurrentPlayer->insightaimmode && !portAdsActive());
    pollAds(0, 1);
    assert(portAdsActive());
    pollAds(0, 0);
    normalAim(1, 1, 1);
    assert(!g_CurrentPlayer->insightaimmode);

    assert(portJoyPortOptionSet("aim_mode", 1));
    normalAim(1, 1, 1);
    controlsLive = 0;
    portAdsTick();
    controlsLive = 1;
    normalAim(0, 0, 1);
    assert(!portAdsActive());
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_CAMERA;
    normalAim(1, 1, 0);
    assert(g_CurrentPlayer->insightaimmode && !portAdsActive());
    normalAim(0, 0, 0);
    selectPlayer(0);

    normalAim(1, 1, 0);
    selectPlayer(1);
    assert(!portAdsActive());
    selectPlayer(0);
    assert(portAdsActive());
    normalAim(0, 0, 0);

    memset(&move, 0, sizeof(move));
    move.controlStickXSafe = -40;
    move.controlStickYSafe = 65;
    move.rLeanLeft = move.crouchDown = 1;
    portAdsConfigureMovement(&move, 0, 0);
    assert(move.canLookAhead && move.canTurnTank);
    assert(move.analogWalk == 65 && move.analogStrafe == -40);
    assert(!move.rLeanLeft && !move.crouchDown);
    portAdsConfigureMovement(&move, 1, 1);
    assert(move.digitalStepForward && move.digitalStepRight);
    assert(!move.digitalStepBack && !move.digitalStepLeft);
    assert(!move.canLookAhead && !move.canTurnTank);
    portAdsConfigureMovement(&move, -1, -1);
    assert(move.digitalStepBack && move.digitalStepLeft);
    assert(!move.digitalStepForward && !move.digitalStepRight);
    portAdsConfigureMovement(&move, 0, 1);
    assert(!move.canLookAhead && move.canTurnTank);
    assert(move.analogStrafe == -40);
    portAdsConfigureMovement(&move, 1, 0);
    assert(move.canLookAhead && !move.canTurnTank);
    assert(move.analogWalk == 65);
    portAdsConfigureMovement(&move, 0, 0);
    assert(!move.digitalStepForward && !move.digitalStepBack);
    assert(!move.digitalStepLeft && !move.digitalStepRight);

    pollAds(0, 1);
    assert(portAdsActive());
    controlsLive = 0;
    assert(!portAdsActive());
    assert(portAdsBlend() == 0.0f);
    portAdsTick();
    controlsLive = 1;
    assert(portAdsBlend() == 0.0f);
    g_PlayerIsInTank = 1;
    assert(!portAdsActive());
    g_PlayerIsInTank = 0;
    g_CurrentPlayer->hand_item[GUNLEFT] = ITEM_WPPKSIL;
    assert(!portAdsActive());
    g_CurrentPlayer->hand_item[GUNLEFT] = ITEM_UNARMED;

    for (i = ITEM_UNARMED; i <= ITEM_CAMERA; i++)
    {
        g_CurrentPlayer->hand_item[GUNRIGHT] = i;
        assert(!!portAdsActive() == ((i >= ITEM_WPPK && i <= ITEM_LASER)
            || i == ITEM_GRENADELAUNCH || i == ITEM_ROCKETLAUNCH || i == ITEM_THROWKNIFE));
    }
    selectPlayer(0);
    for (i = GUN_ANIM_STATE_SWITCH_LOWER; i <= GUN_ANIM_STATE_RELOAD_RAISE; i++)
    {
        g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = i;
        assert(!portAdsActive());
    }
    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_DRY_FIRE;
    assert(portAdsActive());
    selectPlayer(0);

    g_GlobalTimerDelta = 4.5f;
    portAdsTick();
    assert(portAdsBlend() == 0.5f);
    portAdsTick();
    assert(portAdsBlend() == 1.0f);
    pollAds(0, 0);
    portAdsTick();
    assert(portAdsBlend() == 0.5f);
    portAdsTick();
    assert(portAdsBlend() == 0.0f);

    /* Scope presentation follows the sniper transition, including lowering,
     * without entering the original manual-reticle aim mode. */
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_SNIPERRIFLE;
    normalAim(1, 1, 0);
    portAdsTick();
    assert(portAdsActive() && !g_CurrentPlayer->insightaimmode);
    assert(portAdsScopeBlend() == 0.5f);
    portAdsTick();
    assert(portAdsScopeBlend() == 1.0f);
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_AK47;
    assert(portAdsScopeBlend() == 0.0f);
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_SNIPERRIFLE;
    normalAim(0, 0, 0);
    portAdsTick();
    assert(!portAdsActive() && portAdsScopeBlend() == 0.5f);
    controlsLive = 0;
    assert(portAdsScopeBlend() == 0.0f);
    controlsLive = 1;
    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_RELOAD_LOWER;
    assert(portAdsScopeBlend() == 0.0f);
    portAdsTick();
    selectPlayer(0);

    for (i = 0; i < 4; i++)
    {
        s32 tick;
        g_GlobalTimerDelta = 60.0f / rates[i];
        pollAds(0, 1);
        for (tick = 0; tick < rates[i]; tick++) portAdsTick();
        assert(portAdsBlend() == 1.0f);
        pollAds(0, 0);
        assert(!portAdsActive());
        for (tick = 0; tick < rates[i]; tick++) portAdsTick();
        assert(portAdsBlend() == 0.0f);
    }

    pollAds(0, 1);
    selectPlayer(1);
    assert(!portAdsActive());
    assert(portAdsBlend() == 0.0f);
    pollAds(1, 1);
    assert(portAdsActive());
    selectPlayer(0);
    assert(!portAdsActive());

    /* Physical wheel routing is specific to player 1's live sniper ADS, and
     * a burst is consumed once per tick without depending on the tick rate. */
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_SNIPERRIFLE;
    assert(!portAdsMouseWheel(1));
    normalAim(1, 1, 0);
    assert(portAdsMouseWheel(1));
    assert(portAdsMouseWheel(1));
    { const f32 zoom = portAdsWheelZoom(15.0f, 45.0f); assert(zoom > 11.34f && zoom < 11.35f); }
    assert(portAdsWheelZoom(15.0f, 45.0f) == 15.0f);
    for (i = 0; i < 4; i++) {
        f32 zoom;
        g_GlobalTimerDelta = 60.0f / rates[i];
        assert(portAdsMouseWheel(2));
        zoom = portAdsWheelZoom(15.0f, 45.0f);
        assert(zoom > 11.34f && zoom < 11.35f);
        assert(portAdsMouseWheel(-2));
        zoom = portAdsWheelZoom(zoom, 45.0f);
        assert(zoom > 14.999f && zoom < 15.001f);
    }
    assert(portAdsMouseWheel(1));
    { const f32 zoom = portAdsWheelZoom(60.0f, 45.0f); assert(zoom > 39.13f && zoom < 39.14f); }
    assert(portAdsMouseWheel(8));
    assert(portAdsWheelZoom(8.0f, 45.0f) == 7.0f);
    assert(portAdsMouseWheel(-8));
    assert(portAdsWheelZoom(44.0f, 45.0f) == 45.0f);
    for (i = 0; i < 40; i++) assert(portAdsMouseWheel(1));
    { const f32 zoom = portAdsWheelZoom(45.0f, 45.0f); assert(zoom > 14.70f && zoom < 14.72f); }

    selectPlayer(1);
    assert(portAdsMouseWheel(1)); /* player 2 ticking must not redirect the mouse */
    assert(portAdsWheelZoom(15.0f, 45.0f) == 15.0f);
    currentPlayer = 0; g_CurrentPlayer = &players[0];
    assert(portAdsWheelZoom(15.0f, 45.0f) < 15.0f);
    assert(portAdsMouseWheel(1));
    normalAim(0, 0, 0);
    assert(!portAdsMouseWheel(1));
    assert(portAdsWheelZoom(15.0f, 45.0f) == 15.0f);
    normalAim(1, 1, 0);
    assert(portAdsWheelZoom(15.0f, 45.0f) == 15.0f); /* no delayed step on re-entry */
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_AK47;
    assert(!portAdsMouseWheel(1));
    g_CurrentPlayer->hand_item[GUNRIGHT] = ITEM_SNIPERRIFLE;
    controlsLive = 0;
    assert(!portAdsMouseWheel(1));
    controlsLive = 1;
    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_RELOAD_LOWER;
    assert(!portAdsMouseWheel(1));
    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_IDLE;
    assert(portAdsMouseWheel(1));
    testTime += 251ull * 46875ull;
    assert(portAdsWheelZoom(15.0f, 45.0f) == 15.0f);
    normalAim(0, 0, 0);
    assert(portJoyPortOptionSet("aim_mode", 0));
    normalAim(1, 1, 0);
    assert(g_CurrentPlayer->insightaimmode && !portAdsMouseWheel(1));
    pollAds(0, 1); /* the dedicated ADS button works in red-reticle mode too */
    assert(portAdsMouseWheel(1));
    assert(portAdsWheelZoom(15.0f, 45.0f) < 15.0f);
    puts("ADS checks passed: sprint/walk/aim/crouch movement, input, scope, wheel zoom, per-player state, 30/60/120/240 Hz.");
    return 0;
}
