/* ROM-free hit-sound policy checks: which native impact a hit raises, and gating. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../port/src/port_hit_sound.c"

/* The decomp's assert reports through osSyncPrintf; a test failure aborts. */
void osSyncPrintf(const char *fmt, ...)
{
    va_list args; va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args); abort();
}

s32 g_GlobalTimer;
static ALSoundState impact, otherImpact;
static PropRecord guard, otherGuard;
static s32 playerCount = 1, posts, postedVolume;
static ALSoundState *postedState;

s32 getPlayerCount(void) { return playerCount; }
void sndCreatePostEvent(ALSoundState *state, s16 type, s32 value)
{
    assert(type == AL_SNDP_VOL_EVT);
    posts++; postedState = state; postedVolume = value;
}

static void testRaisesTheBulletsImpact(void)
{
    /* Arrange: a bullet's impact on the guard starts this tick. */
    g_GlobalTimer = 10;
    portHitSoundImpact(&impact, &guard);

    /* Act: the same bullet's damage lands. */
    portHitSoundConfirmed(&guard);

    /* Assert: that impact, and only it, plays at point-blank volume once. */
    assert(posts == 1 && postedState == &impact && postedVolume == PORT_HIT_SOUND_FULL);
    portHitSoundConfirmed(&guard);
    assert(posts == 1);
}

static void testEachPelletRaisesItsOwn(void)
{
    /* Shotgun pellets alternate impact and damage within one tick. */
    posts = 0;
    portHitSoundImpact(&impact, &guard);
    portHitSoundConfirmed(&guard);
    portHitSoundImpact(&otherImpact, &guard);
    portHitSoundConfirmed(&guard);
    assert(posts == 2 && postedState == &otherImpact);
}

static void testIgnoresUnrelatedImpacts(void)
{
    posts = 0;

    /* Another character's impact, or one from an earlier tick, is not this hit's. */
    portHitSoundImpact(&impact, &otherGuard);
    portHitSoundConfirmed(&guard);
    portHitSoundImpact(&impact, &guard);
    g_GlobalTimer++;
    portHitSoundConfirmed(&guard);

    /* A hit with no impact (a thrown knife, melee) raises nothing. */
    portHitSoundConfirmed(&guard);
    assert(posts == 0);
}

static void testGating(void)
{
    s32 value;

    posts = 0;
    /* Split-screen and the setting leave the native volume alone. */
    playerCount = 2;
    portHitSoundImpact(&impact, &guard);
    portHitSoundConfirmed(&guard);
    playerCount = 1;
    assert(portHitSoundOptionGet("hit_sounds", &value) && value == 1);
    assert(portHitSoundOptionSet("hit_sounds", 0));
    portHitSoundImpact(&impact, &guard);
    portHitSoundConfirmed(&guard);
    assert(posts == 0);

    /* A suppressed impact is spent, not saved for a later hit. */
    assert(portHitSoundOptionSet("hit_sounds", 1));
    portHitSoundConfirmed(&guard);
    assert(posts == 0);
    portHitSoundImpact(&impact, &guard);
    portHitSoundConfirmed(&guard);
    assert(posts == 1);
    assert(!portHitSoundOptionGet("hit_markers", &value));
    assert(!portHitSoundOptionSet("hit_markers", 0));
}

int main(void)
{
    testRaisesTheBulletsImpact();
    testEachPelletRaisesItsOwn();
    testIgnoresUnrelatedImpacts();
    testGating();
    puts("hit sound tests passed");
    return 0;
}
