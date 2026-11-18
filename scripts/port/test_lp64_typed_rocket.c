/* ROM-free check that the held rocket's view depth is written to its prop's
 * zDepth at each pointer width (LP64-005 Task 4, test_lp64_typed_reads.sh).
 * gun.c is included whole, with stubs for what gunUpdateAttachedRocket calls;
 * --gc-sections drops the rest. */
#include <stdio.h>
#include <stdlib.h>
#include "../../src/game/gun.c"

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lp64_typed_rocket: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

struct player *g_CurrentPlayer;
static PropRecord playerProp;
static Mtxf worldToScreen;
static RenderPosView renderPos;

PropRecord *getCurrentPlayerProp(void) { return &playerProp; }
Mtxf *camGetWorldToScreenMtxf(void) { return &worldToScreen; }
void *dynAllocate(s32 size) { return &renderPos; }
void matrix_4x4_copy(Mtxf *src, Mtxf *dst) { *dst = *src; }
void matrix_4x4_set_position(struct coord3d *position, Mtxf *matrix) {}
void matrix_scalar_multiply(f32 scalar, f32 *matrix) {}
void modelUpdateRelationsQuick(Model *model, ModelNode *parent) {}
void objChangeShading(ObjectRecord *obj, coord3d *pos, Mtxf *mtx, StandTile *stan) {}
void chrobjCollisionRelated(ObjectRecord *obj) {}

/* The rocket ends 5 units in front of the camera. */
void matrix_4x4_multiply_homogeneous(Mtxf *lhs, Mtxf *rhs, Mtxf *result)
{
    ((RenderPosView *) result)->pos.m[3][2] = -5.0f;
}

/* The rocket's prop gets its view depth in zDepth and keeps its position. */
static void testAttachedRocketDepthIsThePropsZDepth(void)
{
    // Arrange: a rocket record held in hand 0, its prop at z = 7.
    static WeaponObjRecord rocket;
    static PropRecord rocketProp;
    static Model rocketModel;
    static ModelFileHeader rocketHeader;
    g_CurrentPlayer = calloc(1, sizeof(*g_CurrentPlayer));
    rocketHeader.numMatrices = 1;
    rocketModel.obj = &rocketHeader;
    rocketModel.scale = 1.0f;
    rocketProp.pos.z = 7.0f;
    rocket.prop = &rocketProp;
    rocket.model = &rocketModel;
    g_CurrentPlayer->hands[0].rocket = (AttachedObj *) &rocket;

    // Act
    gunUpdateAttachedRocket(0);

    // Assert
    expect(rocketProp.zDepth == 5.0f, "the rocket's view depth is written to PropRecord.zDepth");
    expect(rocketProp.pos.z == 7.0f, "the rocket's prop keeps its position");
    expect((rocketProp.flags & 2) != 0, "the rocket's prop gets flag 2");
    free(g_CurrentPlayer);
}

int main(void)
{
    testAttachedRocketDepthIsThePropsZDepth();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_rocket: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
