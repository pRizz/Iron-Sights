/* ROM-free checks of the setup walks that read commands as objects, at each
 * pointer width (LP64-005 Task 4, test_lp64_typed_reads.sh): only an object
 * command's prop is read. loadobjectmodel.c is included whole; at 64 bits the
 * prop converter's size table comes with it, since sizepropdef walks the
 * converted list by it. --gc-sections drops what the tests do not reach. */
#include <stdio.h>
#include <string.h>
#include "../../src/game/loadobjectmodel.c"
#include "../../src/game/cleanup_objects.c"
#if IS_64_BIT
#include "../../port/src/romconv_setup_props.c"
#endif

stagesetup g_CurrentSetup;
u32 g_PortHwWork[PORTHW_WORK_COUNT];

static int failures;
static s32 freedCount;

void objFreePermanently(ObjectRecord *obj, bool freeprop)
{
    freedCount++;
}

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_lp64_typed_setup: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

/* Room for a few commands, aligned for their pointers. */
static uintptr_t commands[512];

/* The command after CMD, by the walk's own step. */
static PropDefHeaderRecord *nextCommand(PropDefHeaderRecord *cmd)
{
    return sizepropdef(cmd) + cmd;
}

static PropDefHeaderRecord *startCommands(void)
{
    memset(commands, 0, sizeof(commands));
    g_CurrentSetup.propDefs = (void *) commands;
    return (PropDefHeaderRecord *) commands;
}

/* A switch that links a door is passed over: the door's index is returned. */
static void testDoorIsFoundPastTheSwitchThatLinksIt(void)
{
    // Arrange: command 0 is a switch whose second link is the door's prop; command 1 the door.
    static PropRecord doorProp;
    PropDefHeaderRecord *cmd = startCommands();
    LinkRecord *link = (LinkRecord *) cmd;
    link->type = PROPDEF_SWITCH;
    link->second = &doorProp;
    cmd = nextCommand(cmd);
    cmd->type = PROPDEF_DOOR;
    ((ObjectRecord *) cmd)->prop = &doorProp;
    nextCommand(cmd)->type = PROPDEF_END;

    // Act
    s32 index = setupGetCommandIndexByProp(&doorProp);

    // Assert
    expect(index == 1, "setupGetCommandIndexByProp reads prop only from object commands");
}

/* The 21 object command types: setupCommandGetObject's object group without UNK41. */
static const s32 objectTypes[] = {
    PROPDEF_DOOR, PROPDEF_PROP, PROPDEF_KEY, PROPDEF_ALARM, PROPDEF_CCTV, PROPDEF_MAGAZINE, PROPDEF_COLLECTABLE,
    PROPDEF_MONITOR, PROPDEF_MULTI_MONITOR, PROPDEF_RACK, PROPDEF_AUTOGUN, PROPDEF_HAT, PROPDEF_AMMO, PROPDEF_ARMOUR,
    PROPDEF_GAS_RELEASING, PROPDEF_VEHICHLE, PROPDEF_AIRCRAFT, PROPDEF_GLASS, PROPDEF_SAFE, PROPDEF_TANK,
    PROPDEF_TINTED_GLASS,
};

static int isObjectType(s32 type)
{
    size_t i;

    for (i = 0; i < sizeof(objectTypes) / sizeof(objectTypes[0]); i++)
    {
        if (objectTypes[i] == type)
        {
            return 1;
        }
    }
    return 0;
}

#if IS_64_BIT
/* At 64 bits only an object command's word at 16 is read as its prop; any
 * other command holding the prop's address there is passed over. (At 32 bits
 * the walk reads every command's bytes 16 to 19, inherited: FORK-089.) */
static void testOnlyObjectCommandsAreReadAsProps(void)
{
    static PropRecord wanted;
    char what[96];
    s32 type;

    for (type = 0; type < PROPDEF_END; type++)
    {
        // Arrange: command 0 of this type holds the prop's address at byte 16, if it has room; command 1 is a door with it.
        PropDefHeaderRecord *cmd = startCommands();
        int room;
        cmd->type = type;
        room = sizepropdef(cmd) * sizeof(u32) >= offsetof(ObjectRecord, prop) + sizeof(void *);
        if (room)
        {
            ((ObjectRecord *) cmd)->prop = &wanted;
        }
        cmd = nextCommand(cmd);
        cmd->type = PROPDEF_DOOR;
        ((ObjectRecord *) cmd)->prop = &wanted;
        nextCommand(cmd)->type = PROPDEF_END;

        // Act
        s32 index = setupGetCommandIndexByProp(&wanted);

        // Assert
        snprintf(what, sizeof(what), "setupGetCommandIndexByProp on a command of type %d", (int) type);
        expect(index == (isObjectType(type) && room ? 0 : 1), what);
    }
}
#endif

/* The types setupCommandGetObject lets fall through to `return obj;` at 32
 * bits, inherited (FORK-089, entry 12): at 64 bits they get NULL. */
static int isFallThroughType(s32 type)
{
    return type == PROPDEF_NOTHING || type == PROPDEF_DEBRIS || type == PROPDEF_UNK16 || type == PROPDEF_UNK41;
}

/* setupCommandGetObject returns a command as an object only for an object type. */
static void testOnlyObjectCommandsAreReturnedAsObjects(void)
{
    char what[96];
    s32 type;

    for (type = 0; type < PROPDEF_END; type++)
    {
        // Arrange: a list of one command of this type.
        PropDefHeaderRecord *cmd = startCommands();
        cmd->type = type;
        nextCommand(cmd)->type = PROPDEF_END;
        const int object = isObjectType(type) || (!IS_64_BIT && isFallThroughType(type));

        // Act
        ObjectRecord *obj = setupCommandGetObject(0, 0);

        // Assert
        snprintf(what, sizeof(what), "setupCommandGetObject on a command of type %d", (int) type);
        expect(obj == (object ? (ObjectRecord *) cmd : NULL), what);
    }
}

/* cleanupObjects frees an object command and, at 64 bits, skips an UNK41 one. */
static void testCleanupFreesOnlyObjects(void)
{
    // Arrange: a prop, an UNK41 record and a prop.
    PropDefHeaderRecord *cmd = startCommands();
    cmd->type = PROPDEF_PROP;
    cmd = nextCommand(cmd);
    cmd->type = PROPDEF_UNK41;
    cmd = nextCommand(cmd);
    cmd->type = PROPDEF_PROP;
    nextCommand(cmd)->type = PROPDEF_END;
    freedCount = 0;

    // Act
    cleanupObjects(0);

    // Assert: the UNK41 record is freed as an object only at 32 bits (inherited).
    expect(freedCount == (IS_64_BIT ? 2 : 3), "cleanupObjects frees the object commands");
}

int main(void)
{
    testOnlyObjectCommandsAreReturnedAsObjects();
    testCleanupFreesOnlyObjects();
    testDoorIsFoundPastTheSwitchThatLinksIt();
#if IS_64_BIT
    testOnlyObjectCommandsAreReadAsProps();
#endif
    if (failures != 0)
    {
        return 1;
    }
    printf("test_lp64_typed_setup: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
