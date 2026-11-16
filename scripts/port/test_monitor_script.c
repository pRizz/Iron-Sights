/* Steps a monitor animation script with the reads that
 * process_monitor_animation_microcode (src/game/propobj.c) uses:
 * monitorScriptCommand finds the command at the cursor and
 * monitorScriptJumpTarget reads a jump's operand (src/game/chrai.h).
 * Built at both pointer widths by scripts/port/test_monitor_script.sh: a
 * script element must hold an address, and the cursor must step by element. */
#include <stdint.h>
#include <stdio.h>
#include <ultra64.h>
#include "chrai.h"

#define CMD_JUMP 0x9
#define CMD_STOP 0xC
#define CMD_ROTATE 0xE
#define ROTATION 0x100

/* Three commands: a two-element rotation, a jump to the third command, and a
 * one-element stop. The script tables in propobj.c are declared the same way. */
static uintptr_t script[5] = {
    MONSETROTATION(ROTATION),
    MONJUMPTO(&script[4]),
    MONSTOPANIM(),
};

static int expect(const char *what, uintptr_t actual, uintptr_t expected)
{
    if (actual == expected) {
        return 1;
    }
    fprintf(stderr, "monitor script: %s is 0x%llx, expected 0x%llx\n", what,
            (unsigned long long) actual, (unsigned long long) expected);
    return 0;
}

int main(void)
{
    MonitorRecord mon = {0};
    struct tvcmd *cmd;

    mon.cmdlist = script;
    mon.offset = 0;
    cmd = monitorScriptCommand(&mon);
    if (!expect("first command", cmd->type, CMD_ROTATE) || !expect("rotation operand", (uintptr_t) cmd->time, ROTATION)) {
        return 1;
    }

    /* The interpreter steps past a two-element command with offset += 2. */
    mon.offset += 2;
    cmd = monitorScriptCommand(&mon);
    if (!expect("second command", cmd->type, CMD_JUMP)) {
        return 1;
    }

    /* As save_ptr_monitor_ani_code_to_obj_ani_slot does for a jump. */
    mon.cmdlist = monitorScriptJumpTarget(cmd);
    mon.offset = 0;
    cmd = monitorScriptCommand(&mon);
    if (!expect("jump destination", (uintptr_t) cmd, (uintptr_t) &script[4]) || !expect("third command", cmd->type, CMD_STOP)) {
        return 1;
    }

    printf("monitor script OK (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
