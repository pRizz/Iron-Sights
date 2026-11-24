/* ROM-free checks that chraction.c reads records through their own types at
 * each pointer width (LP64-005 Task 4, test_lp64_typed_reads.sh). The game
 * file is included whole, as test_stan_callbacks.c does, with stubs for the
 * functions the tested ones call; --gc-sections drops everything else. Each
 * check stores a value in the member a site means and none elsewhere, so a
 * read by position at 64 bits sees zero instead. The attack choosers are also
 * called with no weapon equipped (LP64-006 Task 16), each in a child process,
 * so that a fault is a result rather than the end of the program. */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../../src/game/chraction.c"

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lp64_typed_chraction: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

/* Room for any record read here, so a read by position stays inside it. */
typedef union
{
    ChrRecord chr;
    WeaponObjRecord weapon;
    u8 bytes[2 * sizeof(ChrRecord)];
} RecordRoom;

#define ONE_HANDED_ITEM ITEM_WPPK

u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 mask)
{
    return item == ONE_HANDED_ITEM && mask == WEAPONSTATBITFLAG_ONLY_1_HANDED;
}

/* The functions the animation choosers call, recording what they are given. */
struct ChrModelFileRecord c_item_entries[1];
static u8 animTable[sizeof(struct animation_table_data) + 64];
struct animation_table_data *ptr_animation_table = (struct animation_table_data *) animTable;
static ModelAnimation *lastAnimation, *currentAnimation;
static f32 lastMerge, lastSpeedStartFrame, animFrame, animEndFrame;

PropRecord *chrGetEquippedWeaponProp(ChrRecord *self, GUNHAND hand)
{
    return self->weapons_held[hand];
}

u32 randomGetNext(void)
{
    return 0;
}

void modelSetAnimation(Model *model, ModelAnimation *animation, s32 flip, f32 startframe, f32 speed, f32 merge)
{
    lastAnimation = animation;
    lastMerge = merge;
}

void modelSetAnimSpeed(Model *model, f32 speed, f32 startframe)
{
    lastSpeedStartFrame = startframe;
}

ModelAnimation *objecthandlerGetModelAnim(Model *model)
{
    return currentAnimation;
}

s8 objecthandlerGetModelGunhand(Model *model)
{
    return 0;
}

f32 modelGetAnimFrame(Model *model)
{
    return animFrame;
}

f32 modelGetAnimEndFrame(Model *model)
{
    return animEndFrame;
}

/* Called only on paths these tests do not take. */
ChrRecord *g_ChrSlots;
ALBank *g_musicSfxBufferPtr;

static void unreached(const char *name)
{
    printf("test_lp64_typed_chraction: unexpected call to %s\n", name);
    abort();
}

void chrSetMoving(ChrRecord *guard, s32 moving) { unreached("chrSetMoving"); }
s32 get_numguards(void) { unreached("get_numguards"); return 0; }
void chrobjSndCreatePostEventDefault(ALSoundState *state, coord3d *pos) { unreached("chrobjSndCreatePostEventDefault"); }
void weaponSetGunfireVisible(PropRecord *prop, s32 firing) { unreached("weaponSetGunfireVisible"); }
ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank, s16 index, ALSoundState *state) { unreached("sndPlaySfx"); return NULL; }
s32 stanTestLineUnobstructed(StandTile **tile, f32 x, f32 z, f32 dx, f32 dz, int cdtypes, f32 h, f32 a, f32 b, f32 c)
{
    unreached("stanTestLineUnobstructed");
    return 0;
}

/* What the attack choosers read and call. */
struct weapon_firing_animation_table D_80030078[8];
struct weapon_firing_animation_table D_80030660[8];
point2d D_800309A8, D_800309B0, D_800309C8, D_800309D0, D_800309D8, D_800309E0, D_800309E8;
f32 g_AiReactionSpeed = 1.0f;

s8 bondwalkItemGetAutomaticFiringRate(ITEM_IDS item)
{
    return 0;
}

PropRecord *chrGetEquippedWeaponPropWithCheck(ChrRecord *self, GUNHAND hand)
{
    return self->weapons_held[hand];
}

f32 get_007_reaction_speed(void)
{
    return 0.5f;
}

void modelSetAnimEndFrame(Model *model, f32 endframe)
{
}

/* Referenced by chraction.c's other functions; not reached by these tests. */
ChrRecord *g_ActiveChrs;
s32 g_ActiveChrsCount;
struct player *g_CurrentPlayer;
stagesetup g_CurrentSetup;
ChrRecord *chrFindByLiteralId(s32 index) { unreached("chrFindByLiteralId"); return NULL; }
PropRecord *getCurrentPlayerProp(void) { unreached("getCurrentPlayerProp"); return NULL; }

static f32 subroty;

f32 getsubroty(Model *model)
{
    return subroty;
}

/* weaponIsOneHanded reads the weapon prop's WeaponObjRecord.weaponnum. */
static void testOneHandedReadsTheWeaponNumber(void)
{
    // Arrange
    static RecordRoom room;
    PropRecord prop = {0};
    memset(&room, 0, sizeof(room));
    room.weapon.weaponnum = ONE_HANDED_ITEM;
    prop.type = PROP_TYPE_WEAPON;
    prop.weapon = &room.weapon;

    // Act
    u32 onehanded = weaponIsOneHanded(&prop);

    // Assert
    expect(onehanded != 0, "weaponIsOneHanded reads WeaponObjRecord.weaponnum");
}

/* A guard sent along a course runs at the speed act_gopos.unk59 holds. */
static void testCourseSpeedIsTheGoposSpeed(void)
{
    // Arrange: an unarmed male guard told to sprint (speed 2).
    static RecordRoom room;
    memset(&room, 0, sizeof(room));
    room.chr.actiontype = ACT_GOPOS;
    room.chr.act_gopos.unk59 = SPEED_SPRINT;
    c_item_entries[0].isMale = 1;
    lastAnimation = NULL;

    // Act
    play_hit_soundeffect_and_proper_volume(&room.chr);

    // Assert
    expect(lastAnimation == ANIM_FROM_TABLE(PTR_ANIM_sprinting_one_handed_weapon),
           "play_hit_soundeffect_and_proper_volume reads act_gopos.unk59");
}

/* An aiming guard's shoulders and back follow its firing table's limits. */
static void testAimLimitsComeFromTheFiringTable(void)
{
    // Arrange: aim 1.0 up and -1.0 down against limits of 0.5 and -0.5.
    static RecordRoom up, down;
    struct weapon_firing_animation_table table = {0};
    memset(&up, 0, sizeof(up));
    memset(&down, 0, sizeof(down));
    table.max_up = 0.5f;
    table.max_down = -0.5f;
    table.free_arm_frac_up = 0.25f;
    table.free_arm_frac_down = 0.75f;

    // Act
    chrlvUpdateAimendbackShoulders(&up.chr, &table, 0, 0, 1.0f);
    chrlvUpdateAimendbackShoulders(&down.chr, &table, 0, 0, -1.0f);

    // Assert
    expect(up.chr.aimendback == 0.5f && up.chr.aimendlshoulder == 0.5f && up.chr.aimendrshoulder == 0.125f,
           "chrlvUpdateAimendbackShoulders reads max_up and free_arm_frac_up");
    expect(down.chr.aimendback == -0.5f && down.chr.aimendlshoulder == -0.5f && down.chr.aimendrshoulder == -0.375f,
           "chrlvUpdateAimendbackShoulders reads max_down and free_arm_frac_down");
}

/* A guard whose left-leg death ends jumps backwards, timed by that animation's frame count. */
static void testDeathJumpUsesTheAnimationFrameCount(void)
{
    // Arrange: the jump's header says 120 frames; the left-leg death has ended.
    static RecordRoom room;
    Model model = {0};
    memset(&room, 0, sizeof(room));
    memset(animTable, 0, sizeof(animTable));
    room.chr.model = &model;
    room.chr.act_die.thudframe1 = -1.0f;
    room.chr.act_die.thudframe2 = -1.0f;
    ANIM_FROM_TABLE(PTR_ANIM_jump_backwards)->unk04 = 120;
    currentAnimation = ANIM_FROM_TABLE(PTR_ANIM_death_left_leg);
    animFrame = 10.0f;
    animEndFrame = 5.0f;
    lastSpeedStartFrame = 0.0f;

    // Act
    chrlvTickDie(&room.chr);

    // Assert: (120 - 1) - 50.
    expect(lastAnimation == ANIM_FROM_TABLE(PTR_ANIM_jump_backwards) && lastMerge == 69.0f && lastSpeedStartFrame == 69.0f,
           "chrlvTickDie reads ModelAnimation.unk04");
}

/* Another player's body turns by the firing table's angle_offset. */
static void testRemotePlayerTurnsByTheAngleOffset(void)
{
    // Arrange: a right-handed remote player at subroty 1.0 whose table offsets 0.5.
    static RecordRoom room;
    Model model = {0};
    struct weapon_firing_animation_table table = {0};
    memset(&room, 0, sizeof(room));
    model.gunhand = GUNRIGHT;
    room.chr.model = &model;
    room.chr.actiontype = ACT_BONDMULTI;
    room.chr.act_bondmulti.unk2c = (f32 *) &table;
    table.angle_offset = 0.5f;
    subroty = 1.0f;

    // Act
    f32 turned = chrlvGetSubrotySideback(&room.chr);

    // Assert
    expect(turned == 1.5f, "chrlvGetSubrotySideback reads the firing table's angle_offset");
}

/* A firing table of one entry for each of the 32 directions, for chrlvInitActAttack. */
static struct weapon_firing_animation_table oneFiringAnimation[1];
static struct anim_group_info oneAnimationGroup = {(struct weapon_firing_animation_table (*)[]) oneFiringAnimation, 1};
static struct anim_group_info *firingGroups[32];

/* An unarmed guard (no prop in either hand) told to attack, walk-attack or roll. */
static RecordRoom unarmed;
static Model unarmedModel;

static void prepareUnarmed(void)
{
    u32 i;

    memset(&unarmed, 0, sizeof(unarmed));
    memset(&unarmedModel, 0, sizeof(unarmedModel));
    unarmed.chr.model = &unarmedModel;
    for (i = 0; i < 32; i++)
    {
        firingGroups[i] = &oneAnimationGroup;
    }
}

static void attackWithTheRightHand(void)
{
    point2d hands = {{1, 0}};

    /* attack type 2: the distance is 0, so the direction is the first */
    chrlvInitActAttack(&unarmed.chr, firingGroups, 0, &hands, 2, 0, 0);
}

static void attackWhileWalking(void)
{
    chrlvInitActAttackWalk(&unarmed.chr, 0);
}

static void attackWhileRolling(void)
{
    chrlvInitActAttackRoll(&unarmed.chr, GUNRIGHT);
}

/* How CALL ended in a child process: 0 when it returned, else the signal that ended it. The 32-bit
 * half expects SIGSEGV from a read through NULL (page 0), which is Linux's signal for it; macOS
 * raises SIGBUS there, so the harness assumes Linux (its containers and CI). */
static int signalEnding(void (*call)(void))
{
    int status;
    pid_t child;

    prepareUnarmed();
    fflush(stdout);
    child = fork();
    if (child == 0)
    {
        call();
        _exit(0);
    }
    if (child < 0 || waitpid(child, &status, 0) != child)
    {
        return -1;
    }
    if (WIFSIGNALED(status))
    {
        return WTERMSIG(status);
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

/* chrGetEquippedWeaponProp(...)->chr with no prop: a 64-bit build skips the
 * empty hand; the 32-bit program keeps the inherited read through NULL
 * (decision 10; the owner's patch is t16-chraction-32bit.patch), so its
 * expectation is the fault. */
static void testAnEmptyHand(void (*call)(void), const char *what)
{
    // Arrange: prepareUnarmed, in signalEnding.

    // Act
    int ending = signalEnding(call);

    // Assert
    char line[160];
    snprintf(line, sizeof(line), "%s (ended by %d: 0 returned, %d SIGSEGV, -1 other)", what, ending, SIGSEGV);
#if IS_64_BIT
    expect(ending == 0, line);
#else
    expect(ending == SIGSEGV, line);
#endif
}

int main(void)
{
    testAnEmptyHand(attackWithTheRightHand, "chrlvInitActAttack with no prop in the hand it flags");
    testAnEmptyHand(attackWhileWalking, "chrlvInitActAttackWalk with no prop in either hand");
    testAnEmptyHand(attackWhileRolling, "chrlvInitActAttackRoll with no prop in either hand");
    testRemotePlayerTurnsByTheAngleOffset();
    testDeathJumpUsesTheAnimationFrameCount();
    testAimLimitsComeFromTheFiringTable();
    testOneHandedReadsTheWeaponNumber();
    testCourseSpeedIsTheGoposSpeed();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_chraction: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
