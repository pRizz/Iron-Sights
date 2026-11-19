/* ROM-free check that a remote player's firing table gives its animation
 * whole at each pointer width (LP64-005 strict batch 1, entry 15,
 * test_lp64_typed_reads.sh). bondview2.c is included whole, with stubs for
 * what playerTick calls; --gc-sections drops the rest. The table names an
 * invented animation address, never dereferenced, whose upper half at 64 bits
 * is not the sign extension of its lower half, so a read of the table's first
 * word as an s32 gives another address. */
#include <stdio.h>
#include <stdlib.h>
#include "../../src/game/bondview2.c"

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lp64_typed_player: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

#define LOCAL_PLAYER 0
#define REMOTE_PLAYER 1
#define CROUCH_STANDING 2

/* Data playerTick reads, defined in other game files. */
struct firing_anim_struct firing_animation_groups[4][6];
s32 g_PlayerTickCount;
s32 g_bondviewBondDeathAnimations[1];
s32 g_bondviewBondDeathAnimationsCount;
struct player *g_playerPointers[4];
s32 g_GlobalTickCount;
static u8 animTable[64];
struct animation_table_data *ptr_animation_table = (struct animation_table_data *) animTable;

static ModelAnimation *setAnimation;
static s32 setAnimationCalls;

s32 getPlayerPointerIndex(PropRecord *prop) { return REMOTE_PLAYER; }
s32 get_cur_playernum(void) { return LOCAL_PLAYER; }
s32 get_player_position_in_shuffled(s32 current_player_num) { return 1; }
s32 getPlayerCount(void) { return 2; }
s32 get_debug_render_raster(void) { return DEB_BOND_VIEW; }
s32 playerGetCrouchPos(s32 index) { return CROUCH_STANDING; }
PropRecord *chrGetEquippedWeaponProp(ChrRecord *arg0, GUNHAND arg1) { return NULL; }
u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 mask) { return 0; }
u32 randomGetNext(void) { return 0; }
s32 chrTick(PropRecord *prop) { return TICKOP_NONE; }
void chrSetFiring(ChrRecord *arg0, s32 hand, s32 firing) {}
s32 sub_GAME_7F02D630(ChrRecord *arg0, GUNHAND hand, coord3d *arg2) { return 0; }
void chrlvUpdateAimendbackShoulders(ChrRecord *arg0, void *arg1, s32 same, s32 swap, f32 next) {}
void getsuboffset(Model *objinst, coord3d *offset) {}
void setsuboffset(Model *objinst, coord3d *offset) {}
void setsubroty(Model *model, f32 angle) {}
void modelSetAnimLooping(Model *model, f32 loopframe, f32 loopmerge) {}
void modelSetAnimEndFrame(Model *model, f32 endframe) {}
void modelSetAnimSpeed(Model *model, f32 speed, f32 startframe) {}

/* Only the local player's path calls these; the test does not take it. */
void chrDetectRooms(ChrRecord *self) { abort(); }
void chrpropRegisterRoom(PropRecord *prop, s16 room) { abort(); }
void chrpropDeregisterRoom(PropRecord *prop, s16 room) { abort(); }
Mtxf *currentPlayerGetViewToWorldMtxf(void) { abort(); }
void matrix_4x4_multiply_homogeneous(Mtxf *lhs, Mtxf *rhs, Mtxf *result) { abort(); }

void modelSetAnimation(Model *model, ModelAnimation *animation, s32 flip, f32 startframe, f32 speed, f32 merge)
{
    setAnimation = animation;
    setAnimationCalls++;
}

/* An unarmed remote player stepping sideways, standing, takes its group's
 * sidestep entry, whose table names the animation by address. */
static void testSidestepAnimationIsTheTablesAddress(void)
{
    // Arrange
    static struct weapon_firing_animation_table table;
    static ChrRecord chr;
    static PropRecord prop;
    static Model bodyModel;
    static struct player *players[2];
    ModelAnimation *animation = (ModelAnimation *) (uintptr_t) 0x0000123456789AB0ull;
    table.anim.anim = animation;
    firing_animation_groups[2][3].pointer = &table;
    firing_animation_groups[2][3].x = 1.0f;
    firing_animation_groups[2][3].y = -1.0f;
    firing_animation_groups[2][3].z = -1.0f;
    g_PlayerTickCount = 2;
    players[REMOTE_PLAYER] = calloc(1, sizeof(struct player));
    players[REMOTE_PLAYER]->bodyModel = &bodyModel;
    players[REMOTE_PLAYER]->cameramode = 0;
    players[REMOTE_PLAYER]->speedsideways = 0.5f;
    g_playerPointers[REMOTE_PLAYER] = players[REMOTE_PLAYER];
    prop.chr = &chr;

    // Act
    playerTick(&prop);

    // Assert
    expect(setAnimationCalls == 1, "playerTick sets the remote player's animation once");
    expect(setAnimation == animation, "playerTick reads the firing table's animation whole");
    expect(players[REMOTE_PLAYER]->players_cur_animation == (intptr_t) animation,
           "playerTick keeps the whole address as the current animation");
    free(players[REMOTE_PLAYER]);
}

int main(void)
{
    testSidestepAnimationIsTheTablesAddress();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_player: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
