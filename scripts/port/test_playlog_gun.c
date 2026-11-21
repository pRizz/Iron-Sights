/* ROM-free check of the play log's gun line and GE_PLAYSTALL (LP64-006 Task 1,
 * run by test_inputrec_headless.sh). port_playlog.c is linked as its own
 * object, so this file's portPlaySleepMs replaces its weak one; the game state
 * it reads (player 0's record, the frame timers, pad 0, the input tick) is
 * fabricated here. Each scenario runs in its own child with its stdout in a
 * file, since port_playlog.c reads GE_PLAYLOG and GE_PLAYSTALL once a
 * process. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ultra64.h>
#include <bondconstants.h>
#include "game/bondview.h"
#include "game/player.h"
#include "port_playlog.h"

static struct player sPlayer;
static struct player_data sPerm;
struct player *g_CurrentPlayer = &sPlayer;
struct player_data *g_playerPerm = &sPerm;
s32 g_ClockTimer;
s32 g_GlobalTimer;
static u32 sTick;
static u16 sPad0;
static int sSleeps;
static u32 sSleptMs;
static int sHurts;
static f32 sHurtDamage;

u32 portInputRecTickNow(void)
{
    return sTick;
}

s32 get_cur_playernum(void)
{
    return 0;
}

void bondviewKillCurrentPlayer(void)
{
}

void record_damage_kills(f32 damage, f32 vectorx, f32 vectorz, s32 playerid, s32 affectsArmor)
{
    (void) vectorx;
    (void) vectorz;
    (void) playerid;
    (void) affectsArmor;
    sHurts++;
    sHurtDamage = damage;
}

u16 joyGetButtons(s8 contpadnum, u16 mask)
{
    return contpadnum == 0 ? (sPad0 & mask) : 0;
}

void portPlaySleepMs(u32 ms)
{
    sSleeps++;
    sSleptMs = ms;
}

static int sFailures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_playlog_gun: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        sFailures++;
    }
}

static void setHand(struct hand *hand, s32 item, s32 state, s32 anim, s8 firing, s32 hold, s32 ammo, s32 next)
{
    hand->weaponnum = item;
    hand->weapon_action_state = state;
    hand->weapon_current_animation = anim;
    hand->weapon_firing_status = firing;
    hand->weapon_hold_time = hold;
    hand->weapon_ammo_in_magazine = ammo;
    hand->weapon_next_weapon = next;
}

/* Runs BODY in a child with GE_PLAYLOG=LOG and, when VARIABLE is not NULL,
 * VARIABLE=VALUE (GE_PLAYLOG unset when LOG is NULL), its stdout in OUT; true
 * when the child reported no failure. */
static int inChild(const char *name, const char *log, const char *variable, const char *value, int (*body)(void),
                   char *out, size_t outSize)
{
    char path[] = "/tmp/midas-playlog-gun-XXXXXX";
    int fd = mkstemp(path);
    pid_t child;
    int status;
    FILE *file;
    size_t got;

    if (fd < 0)
    {
        perror("mkstemp");
        return 0;
    }
    fflush(stdout);
    child = fork();
    if (child < 0)
    {
        perror("fork");
        return 0;
    }
    if (child == 0)
    {
        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            _exit(2);
        }
        unsetenv("GE_PLAYSTALL");
        unsetenv("GE_PLAYHURT");
        log != NULL ? setenv("GE_PLAYLOG", log, 1) : unsetenv("GE_PLAYLOG");
        if (variable != NULL)
        {
            setenv(variable, value, 1);
        }
        status = body();
        fflush(stdout); /* _exit skips stdio */
        _exit(status);
    }
    close(fd);
    if (waitpid(child, &status, 0) != child)
    {
        status = -1;
    }
    file = fopen(path, "r");
    got = file != NULL ? fread(out, 1, outSize - 1, file) : 0;
    out[got] = '\0';
    if (file != NULL)
    {
        fclose(file);
    }
    unlink(path);
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        printf("test_playlog_gun: FAIL (%d-bit): %s (child)\n%s", (int) (sizeof(void *) * 8), name, out);
        return 0;
    }
    return 1;
}

static int printGunLine(void)
{
    // Arrange: both hands hold item 11 in the same state
    setHand(&g_CurrentPlayer->hands[GUNRIGHT], 11, 3, 7, 1, 40, 30, -1);
    setHand(&g_CurrentPlayer->hands[GUNLEFT], 11, 3, 7, 1, 40, 30, -1);
    g_ClockTimer = 2;
    g_GlobalTimer = 345;

    // Act
    portPlayGunLine(77);
    return 0;
}

static int stallAround100(void)
{
    int bad = 0;

    // Act and Assert: player 0's passes at ticks 99, 100 (twice) and 101
    sTick = 99;
    portPlayPlayerTick();
    bad |= sSleeps != 0;
    sTick = 100;
    portPlayPlayerTick();
    portPlayPlayerTick();
    bad |= sSleeps != 1 || sSleptMs != 250;
    sTick = 101;
    portPlayPlayerTick();
    bad |= sSleeps != 1;
    return bad;
}

static int hurtAround100(void)
{
    int bad = 0;

    // Act and Assert: player 0's passes at ticks 99, 100 (twice) and 101
    sTick = 99;
    portPlayPlayerTick();
    bad |= sHurts != 0;
    sTick = 100;
    portPlayPlayerTick();
    portPlayPlayerTick();
    bad |= sHurts != 1 || sHurtDamage <= 0.0f;
    sTick = 101;
    portPlayPlayerTick();
    bad |= sHurts != 1 || sSleeps != 0;
    return bad;
}

static void gunLineHasBothHands(void)
{
    char out[512];

    // Act
    int ran = inChild("gun_line_has_both_hands", "2", NULL, NULL, printGunLine, out, sizeof(out));

    // Assert
    expect(ran && strcmp(out, "ge: play: gun tick=77 clock=2 global=345 r=11,3,7,1,40,30,-1 "
                              "l=11,3,7,1,40,30,-1\n") == 0,
           "gun_line_has_both_hands");
}

static void levelOnePrintsNoGunLine(void)
{
    char out[512];

    // Act
    int ran = inChild("level_one_prints_no_gun_line", "1", NULL, NULL, printGunLine, out, sizeof(out));

    // Assert
    expect(ran && out[0] == '\0', "level_one_prints_no_gun_line");
}

static void stallSleepsOnceAtItsTick(void)
{
    char out[512];

    // Arrange: GE_PLAYSTALL=100:250, no play log

    // Act and Assert: one sleep of 250 ms at tick 100, none at 99 or 101
    expect(inChild("stall_sleeps_once_at_its_tick", NULL, "GE_PLAYSTALL", "100:250", stallAround100, out,
                   sizeof(out)),
           "stall_sleeps_once_at_its_tick");
}

static void hurtHitsPlayerZeroOnceAtItsTick(void)
{
    char out[512];

    // Arrange: GE_PLAYHURT=100, no play log

    // Act and Assert: one hit at tick 100, none at 99 or 101, and no sleep
    expect(inChild("hurt_hits_player_zero_once_at_its_tick", NULL, "GE_PLAYHURT", "100", hurtAround100, out,
                   sizeof(out)),
           "hurt_hits_player_zero_once_at_its_tick");
}

int main(void)
{
    gunLineHasBothHands();
    levelOnePrintsNoGunLine();
    stallSleepsOnceAtItsTick();
    hurtHitsPlayerZeroOnceAtItsTick();
    if (sFailures)
    {
        return 1;
    }
    printf("test_playlog_gun: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
