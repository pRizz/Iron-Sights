/* Included by test_ads.c (after test_ads_accuracy.c, whose ramromFlag stands
 * in for demo playback): Noclip (FORK-072) tilts the walk step along the
 * view, flies only when allowed and toggled, follows the floor below and
 * lands on it, or where it last flew over one. */
#include "../../port/src/port_noclip.c"

static StandTile noclipFloor;
static f32 noclipFloorY;
static s32 noclipFloorPresent;
static s32 noclipRoomCalls;
static s32 noclipRoomEntered = -1;
static s32 noclipPopups;

StandTile *stanFindTileBelowPos(coord3d *pos, u8 *rooms, f32 *yRtn)
{
    (void) rooms;
    if (!noclipFloorPresent || pos->f[1] < noclipFloorY) return NULL;
    if (yRtn != NULL) *yRtn = noclipFloorY;
    return &noclipFloor;
}
void bondviewUpdatePlayerRoom(struct player *player) { (void) player; noclipRoomCalls++; }
void objectivestatusCheckRoomEntered(s32 roomid) { noclipRoomEntered = roomid; }
void portConPrintf(s32 showmsg, const char *fmt, ...) { (void) fmt; noclipPopups += showmsg != 0; }
s32 portConIsOpen(void) { return 0; } /* joy_port.c's crouch step */
static s32 noclipFogReloads;
static f32 noclipFogScale = 1.0f;
void portFogReapply(void) { noclipFogReloads++; noclipFogScale = portNoclipViewScaleApply(); portFogApply(); } /* bgfog.c */

static void noclipPress(void)
{
    portJoyPortPadDown(0, PORTACT_NOCLIP);
    portJoyPortPollCommit();
    portJoyPortPollCommit(); /* the release, so the next press is a new edge */
}

static void testNoclip(void)
{
    const coord3d forward = { 0.0f, 0.0f, 10.0f };
    const coord3d sideways = { 10.0f, 0.0f, 0.0f };
    const s32 savedRamrom = ramromFlag;
    struct player *pl;
    coord3d out;
    s32 setting = -1;

    /* The pure step, facing +z: level walks stay level, the forward part
     * tilts with the pitch and the sideways part never does. */
    portNoclipFlyStep(&forward, 0.0f, 1.0f, 0.0f, 1.0f, &out);
    assert(out.f[0] == 0.0f && out.f[1] == 0.0f && out.f[2] == 10.0f);
    portNoclipFlyStep(&forward, 0.0f, 1.0f, 1.0f, 0.0f, &out);
    assert(out.f[1] == 10.0f && out.f[2] == 0.0f);
    portNoclipFlyStep(&forward, 0.0f, 1.0f, -0.5f, 0.8660254f, &out);
    assert(fabsf(out.f[1] + 5.0f) < 0.001f && fabsf(out.f[2] - 8.660254f) < 0.001f);
    portNoclipFlyStep(&sideways, 0.0f, 1.0f, 1.0f, 0.0f, &out);
    assert(out.f[0] == 10.0f && out.f[1] == 0.0f && out.f[2] == 0.0f);
    portNoclipFlyStep(&forward, -1.0f, 0.0f, 1.0f, 0.0f, &out); /* facing -x, the walk is sideways */
    assert(out.f[1] == 0.0f && out.f[2] == 10.0f);

    /* Gating. */
    assert(portNoclipFliesFor(TRUE, TRUE, FALSE, FALSE, FALSE));
    assert(!portNoclipFliesFor(FALSE, TRUE, FALSE, FALSE, FALSE));
    assert(!portNoclipFliesFor(TRUE, FALSE, FALSE, FALSE, FALSE));
    assert(!portNoclipFliesFor(TRUE, TRUE, TRUE, FALSE, FALSE));
    assert(!portNoclipFliesFor(TRUE, TRUE, FALSE, TRUE, FALSE));
    assert(!portNoclipFliesFor(TRUE, TRUE, FALSE, FALSE, TRUE));

    /* The setting and the bind: off by default, N on the keyboard,
     * appended after the watch controls so saved bindings keep their slots. */
    assert(portJoyPortOptionGet("allow_noclip", &setting) && setting == PORT_NOCLIP_ALLOW_DEFAULT);
    assert(portOptFind("allow_noclip") >= 0 && portOptIsToggle(portOptFind("allow_noclip")));
    assert(PORTACT_NOCLIP == 28 && strcmp(portJoyPortBindDefault(PORTACT_NOCLIP), "N") == 0);
    assert(PORTACT_NOCLIP_UP == 29 && strcmp(portJoyPortBindDefault(PORTACT_NOCLIP_UP), "Space") == 0);
    assert(PORTACT_NOCLIP_DOWN == 30 && strcmp(portJoyPortBindDefault(PORTACT_NOCLIP_DOWN), "") == 0);

    selectPlayer(0);
    pl = g_CurrentPlayer;
    controlsLive = 1;
    ramromFlag = 0;
    g_PlayerIsInTank = 0;
    pl->bonddead = 0;
    pl->field_2A6C = 0;
    pl->field_488.current_tile_ptr = &noclipFloor;
    pl->field_488.collision_position.f[0] = 100.0f;
    pl->field_488.collision_position.f[2] = 200.0f;
    pl->field_70 = 0.0f;
    pl->field_488.theta_transform.f[0] = 0.0f;
    pl->field_488.theta_transform.f[2] = 1.0f;
    noclipFloor.room = 7;
    noclipFloorY = 0.0f;
    noclipFloorPresent = 1;
    portNoclipResetPlayer(0);

    /* Not allowed: the press only says how to allow it. */
    noclipPopups = 0;
    noclipPress();
    portNoclipTick();
    assert(!portNoclipActive() && noclipPopups == 1);

    /* Allowed: a press flies, straight up while looking up, at 3x the walk step. */
    assert(portJoyPortOptionSet("allow_noclip", 1));
    noclipPress();
    portNoclipTick();
    assert(portNoclipActive());
    pl->vv_sinverta = 1.0f;
    pl->vv_cosverta = 0.0f;
    pl->field_7C = -3.0f;
    noclipRoomCalls = 0;
    portNoclipMove(&forward);
    assert(pl->field_70 == 30.0f && pl->field_7C == 0.0f);
    assert(pl->field_488.collision_position.f[2] == 200.0f && pl->bondprevpos.f[2] == 200.0f);
    assert(pl->field_488.current_tile_ptr == &noclipFloor && noclipRoomCalls == 1 && noclipRoomEntered == 7);

    /* Above no floor it keeps the last tile; landing there with none below
     * puts him back where he last flew over one. */
    pl->vv_sinverta = 0.0f;
    pl->vv_cosverta = 1.0f;
    noclipFloorPresent = 0;
    portNoclipMove(&forward);
    portNoclipMove(&forward);
    assert(pl->field_488.collision_position.f[2] == 260.0f && pl->field_488.current_tile_ptr == &noclipFloor);
    noclipPress();
    portNoclipTick();
    assert(!portNoclipActive());
    assert(pl->field_488.collision_position.f[2] == 200.0f && pl->field_70 == 30.0f);

    /* With a floor below, landing leaves him where he is to fall onto it. */
    noclipPress();
    portNoclipTick();
    noclipFloorPresent = 1;
    portNoclipMove(&forward);
    noclipPress();
    portNoclipTick();
    assert(!portNoclipActive() && pl->field_488.collision_position.f[2] == 230.0f && pl->field_70 == 30.0f);

    /* Speeds and the rise / fall controls: Walk / Sprint makes it 12x and the
     * native move scale stays 1 (no racing walk animation); Space rises,
     * Crouch Hold or Noclip Down falls, both cancel; Crouch Hold does not
     * crouch while flying. */
    {
        const coord3d still = { 0.0f, 0.0f, 0.0f };
        const f32 savedDelta = g_GlobalTimerDelta;

        g_GlobalTimerDelta = 1.0f;
        noclipPress();
        portNoclipTick();
        assert(portNoclipActive());
        pl->field_70 = 0.0f;
        pl->field_488.collision_position.f[2] = 0.0f;
        portJoyPortPadDown(0, PORTACT_WALK);
        portJoyPortPollCommit();
        assert(portJoyPortMoveScale(0) == 1.0f);
        portNoclipMove(&forward);
        assert(pl->field_488.collision_position.f[2] == 120.0f);
        portJoyPortPadDown(0, PORTACT_WALK);
        portJoyPortPadDown(0, PORTACT_NOCLIP_UP);
        portJoyPortPollCommit();
        portNoclipMove(&still);
        assert(pl->field_70 == PORT_NOCLIP_CLIMB * PORT_NOCLIP_SPRINT_SPEED);
        portJoyPortPadDown(0, PORTACT_CROUCH_HOLD);
        portJoyPortPollCommit();
        pl->crouchpos = CROUCH_STAND;
        assert(portJoyPortCrouchStep() == 0);
        portNoclipMove(&still);
        assert(pl->field_70 == PORT_NOCLIP_CLIMB * PORT_NOCLIP_SPEED * 3.0f);
        portJoyPortPadDown(0, PORTACT_NOCLIP_UP);
        portJoyPortPadDown(0, PORTACT_NOCLIP_DOWN);
        portJoyPortPollCommit();
        portNoclipMove(&still);
        assert(pl->field_70 == PORT_NOCLIP_CLIMB * PORT_NOCLIP_SPEED * 3.0f);
        portJoyPortPollCommit(); /* everything released */
        noclipPress();
        portNoclipTick();
        assert(!portNoclipActive());
        g_GlobalTimerDelta = savedDelta;
    }
    assert(portNoclipClimbFor(-1, TRUE, 0.5f) == -PORT_NOCLIP_CLIMB * PORT_NOCLIP_SPRINT_SPEED * 0.5f);

    /* View distance: every room while flying, and the fog reloads at the
     * Noclip View Distance multiplier when flight starts or the setting
     * changes, and back at 1 on landing. */
    {
        s32 view = -1;

        assert(portNoclipViewClamp(0) == 1 && portNoclipViewClamp(99) == PORT_NOCLIP_VIEW_MAX);
        /* rooms out of the 16.16 room-matrix range are not drawn while flying */
        assert(portNoclipMatrixOffsetFits(31000.0f, -31000.0f, 0.0f));
        assert(!portNoclipMatrixOffsetFits(0.0f, 0.0f, -40000.0f) && !portNoclipMatrixOffsetFits(33000.0f, 0.0f, 0.0f));
        assert(portNoclipViewScaleFor(FALSE, 10) == 1.0f && portNoclipViewScaleFor(TRUE, 4) == 4.0f);
        assert(portJoyPortOptionGet("noclip_view", &view) && view == PORT_NOCLIP_VIEW_DEFAULT);
        assert(portOptFind("noclip_view") >= 0 && portOptValueCount(portOptFind("noclip_view")) == 3);
        assert(!portNoclipAllRooms());
        noclipFogReloads = 0;
        noclipPress();
        portNoclipTick();
        assert(portNoclipAllRooms() && noclipFogReloads == 1 && noclipFogScale == 4.0f);
        portNoclipTick();
        assert(noclipFogReloads == 1); /* nothing changed */
        assert(portJoyPortOptionSet("noclip_view", 10));
        portNoclipTick();
        assert(noclipFogReloads == 2 && noclipFogScale == 10.0f);
        assert(portJoyPortOptionSet("noclip_view", 1));
        portNoclipTick();
        assert(noclipFogScale == 1.0f && portNoclipAllRooms());
        assert(portJoyPortOptionSet("noclip_view", PORT_NOCLIP_VIEW_DEFAULT));
        portNoclipTick();
        noclipPress();
        portNoclipTick();
        assert(!portNoclipAllRooms() && noclipFogScale == 1.0f);
    }

    /* Demo playback, the tank and turning the setting off all land him. */
    noclipPress();
    portNoclipTick();
    assert(portNoclipActive());
    ramromFlag = 1;
    portNoclipTick();
    assert(!portNoclipActive());
    ramromFlag = 0;
    portNoclipTick();
    assert(portNoclipActive());
    g_PlayerIsInTank = 1;
    portNoclipTick();
    assert(!portNoclipActive());
    g_PlayerIsInTank = 0;
    portNoclipTick();
    assert(portNoclipActive());
    assert(portJoyPortOptionSet("allow_noclip", 0));
    portNoclipTick();
    assert(!portNoclipActive());

    /* A new stage starts on foot. */
    assert(portJoyPortOptionSet("allow_noclip", 1));
    noclipPress();
    portNoclipTick();
    assert(portNoclipActive());
    portNoclipResetPlayer(0);
    assert(!portNoclipActive());
    assert(portJoyPortOptionSet("allow_noclip", PORT_NOCLIP_ALLOW_DEFAULT));
    ramromFlag = savedRamrom;
}

/* Leaning (FORK-073) and Fog (FORK-074), kept here for test_ads.c's line budget. */
#include "test_lean.c"
#include "test_fog.c"
