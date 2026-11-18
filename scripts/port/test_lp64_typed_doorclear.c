/* ROM-free check that a moving door's clearance test reads a guard's hidden
 * flag from its ChrRecord at each pointer width (LP64-005 Task 5,
 * test_lp64_typed_reads.sh). propobj.c is included whole, with stubs for what
 * sub_GAME_7F0448A8 calls; --gc-sections drops the rest. Each check sets the
 * guard's chrflags and its prop pointer to opposite answers, so a read of
 * ObjectRecord.model, which overlays ChrRecord.prop at 64 bits, fails; a
 * third puts a viewer without a body (chr NULL) in the swing. */
#include <stdio.h>
#include <stdlib.h>
#include "../../src/game/propobj.c"

static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lp64_typed_doorclear: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

#define DOOR_PROP 1
#define GUARD_PROP 2

PropRecord g_Props[4];
s16 *ptr_list_object_lookup_indices;
static s16 lookup[] = {DOOR_PROP, GUARD_PROP, -1};

/* The door's swing: a square of side 20 around the origin, from y 0 to 100. */
static struct rect4f doorPolygon = {.f = {-10.0f, -10.0f, 10.0f, -10.0f, 10.0f, 10.0f, -10.0f, 10.0f}};

void chraiGetCollisionBounds(PropRecord *prop, struct rect4f **polygon, s32 *edges, f32 *top, f32 *bottom)
{
    *polygon = &doorPolygon;
    *edges = 4;
    *top = 100.0f;
    *bottom = 0.0f;
}

void chraiGetPropRoomIds(PropRecord *self, s32 *roomids) { roomids[0] = -1; }
void roomGetProps(s32 *roomids) { ptr_list_object_lookup_indices = lookup; }
f32 sub_GAME_7F03CFE8(PropRecord *prop) { return 0.0f; }

/* chrobjTestPointPolygonCollision's callees: not reached, since the guard
 * already stands inside the swing. */
f32 distBetweenPoints2d(f32 o_x, f32 o_z, f32 p_x, f32 p_z) { abort(); }
bool stanPointProjectsOntoEdge(f32 x1, f32 z1, f32 x2, f32 z2, f32 x3, f32 z3) { abort(); }
f32 stanGetSignedPointLineDistance(f32 x1, f32 z1, f32 x2, f32 z2, f32 x3, f32 z3) { abort(); }

/* A guard 10 units across and 50 tall. */
void chrpropGetCollisionBounds(PropRecord *prop, f32 *radius, f32 *height, f32 *bottom)
{
    *radius = 5.0f;
    *height = 50.0f;
    *bottom = 0.0f;
}

/* The guard stands inside the swing. */
s32 chrpropTestPointInPolygon(coord3d *point, struct rect4f *polygon, s32 edges) { return 1; }

static ChrRecord guard;

/* A door prop and a guard prop standing in its swing; the guard's prop
 * pointer is invented and never followed. */
static void arrangeDoorAndGuard(u32 chrflags, uintptr_t guardPropWord)
{
    g_Props[DOOR_PROP].type = PROP_TYPE_DOOR;
    g_Props[GUARD_PROP].type = PROP_TYPE_CHR;
    g_Props[GUARD_PROP].chr = &guard;
    guard.chrflags = chrflags;
    guard.prop = (PropRecord *) guardPropWord;
    guard.hidden = 0;
}

static void testAVisibleGuardInTheSwingBlocksTheDoor(void)
{
    // Arrange: chrflags without CHRFLAG_HIDDEN; a prop pointer with that bit set.
    arrangeDoorAndGuard(0, CHRFLAG_HIDDEN);

    // Act
    s32 clear = sub_GAME_7F0448A8(&g_Props[DOOR_PROP]);

    // Assert
    expect(clear == 0, "a guard without CHRFLAG_HIDDEN in the door's swing blocks it");
}

static void testAHiddenGuardInTheSwingDoesNotBlockTheDoor(void)
{
    // Arrange: chrflags with CHRFLAG_HIDDEN; a prop pointer without that bit.
    arrangeDoorAndGuard(CHRFLAG_HIDDEN, 0);

    // Act
    s32 clear = sub_GAME_7F0448A8(&g_Props[DOOR_PROP]);

    // Assert
    expect(clear == 1, "a guard with CHRFLAG_HIDDEN in the door's swing does not block it");
}

static void testABodilessViewerInTheSwingBlocksTheDoor(void)
{
    // Arrange: a viewer prop without a body (chr NULL, as bondview_r.c stores
    // it), standing in the door's swing.
    g_Props[DOOR_PROP].type = PROP_TYPE_DOOR;
    g_Props[GUARD_PROP].type = PROP_TYPE_VIEWER;
    g_Props[GUARD_PROP].chr = NULL;

    // Act
    s32 clear = sub_GAME_7F0448A8(&g_Props[DOOR_PROP]);

    // Assert
    expect(clear == 0, "a viewer without a body in the door's swing blocks it");
}

int main(void)
{
    testAVisibleGuardInTheSwingBlocksTheDoor();
    testAHiddenGuardInTheSwingDoesNotBlockTheDoor();
    testABodilessViewerInTheSwingBlocksTheDoor();
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_doorclear: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
