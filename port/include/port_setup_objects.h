#ifndef PORT_SETUP_OBJECTS_H
#define PORT_SETUP_OBJECTS_H
/* The setup command types that are ObjectRecords (LP64-005 Task 4): the
 * object group of setupCommandGetObject (loadobjectmodel.c) without UNK41.
 * Only these have a prop at offset 16; at 64 bits another command's bytes
 * there are its own members (a switch's second link) or the next command's.
 * Used inside #if IS_64_BIT only: the 32-bit walks are left as inherited. */
#include "bondconstants.h"
#include "platform_info.h"

static inline int portSetupTypeIsObject(s32 type)
{
    switch (type)
    {
        case PROPDEF_DOOR:
        case PROPDEF_PROP:
        case PROPDEF_KEY:
        case PROPDEF_ALARM:
        case PROPDEF_CCTV:
        case PROPDEF_MAGAZINE:
        case PROPDEF_COLLECTABLE:
        case PROPDEF_MONITOR:
        case PROPDEF_MULTI_MONITOR:
        case PROPDEF_RACK:
        case PROPDEF_AUTOGUN:
        case PROPDEF_HAT:
        case PROPDEF_AMMO:
        case PROPDEF_ARMOUR:
        case PROPDEF_GAS_RELEASING:
        case PROPDEF_VEHICHLE:
        case PROPDEF_AIRCRAFT:
        case PROPDEF_GLASS:
        case PROPDEF_SAFE:
        case PROPDEF_TANK:
        case PROPDEF_TINTED_GLASS:
            return 1;
    }
    return 0;
}

#endif
