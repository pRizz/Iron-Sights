/*
 * indyrescmdCheckFileExists copies a file name into its command's 256-byte
 * field with the game's own strncpy (src/str.c). That strncpy decrements its
 * count only for the characters before the terminator, so asked for the whole
 * field it writes one byte past it for any shorter name: under the sanitizer
 * that is a stack-buffer-overflow. Every build also checks the command sent.
 *
 * fileGetIndex asks the absent host about a file the ROM lacks; the answer
 * indycommHostCheckFileExists returns was never written, so whether a 64-bit
 * build registered a dangling name or stopped on index 0 depended on what the
 * stack held. At 64 bits the answer is now no file, whatever the stack holds.
 */
#include "src/str.c"
#include "src/game/indy_commands.c"
#include "src/game/indy_comms.c"
#include "test_buffer_helpers.h"

static u8 sentCommand[sizeof(indy_resource_entry_type5) + 8];
static u32 sentSize;

void osWriteHost(void *buffer, u32 size)
{
    assert(size <= sizeof(sentCommand));
    for (u32 i = 0; i < size; i++) sentCommand[i] = ((u8 *)buffer)[i];
    sentSize = size;
}

void osReadHost(void *buffer, u32 size) { (void)buffer; (void)size; }

/* Fails unless the field sent holds NAME's first characters, then zeros. */
static void assertFieldHolds(const char *name, u32 kept)
{
    const indy_resource_entry_type5 *cmd = (const indy_resource_entry_type5 *)sentCommand;
    assert(sentSize == sizeof(indy_resource_entry_type5));
    assert(cmd->entry.type == INDY_SENDCHECKFILEEXISTS);
    assert(cmd->entry.size == (s32)sizeof(indy_resource_entry_type5));
    for (u32 i = 0; i < sizeof(cmd->strbuffer); i++) {
        assert(cmd->strbuffer[i] == (i < kept ? (u8)name[i] : 0));
    }
}

/* Leaves nonzero bytes where the next call's frame will be. */
__attribute__((noinline)) static void dirtyTheStack(void)
{
    volatile u8 garbage[4096];
    for (u32 i = 0; i < sizeof(garbage); i++) garbage[i] = 0xA5;
}

__attribute__((noinline)) static u8 *checkOnADirtyStack(char *name)
{
    s32 size;
    dirtyTheStack();
    return indycommHostCheckFileExists(name, &size);
}

static void testAMissGetsNoAnswerFromAnAbsentHost(void)
{
#if IS_64_BIT
    /* Arrange: the link up, as indycommInit leaves it, and a dirty stack. */
    indy_ready = 1;

    /* Act */
    u8 *answer = checkOnADirtyStack("UsetupnoneZ");

    /* Assert */
    if (answer != NULL) {
        fprintf(stderr, "FAIL: indycommHostCheckFileExists answered %p for a file no host has\n", (void *)answer);
        exit(1);
    }
    indy_ready = 0;
#endif
}

int main(void)
{
    /* Arrange: a 12-character name, and one longer than the field. */
    const char *shortName = "twelve_chars";
    char longName[300];
    for (u32 i = 0; i < sizeof(longName) - 1; i++) longName[i] = 'a' + i % 26;
    longName[sizeof(longName) - 1] = '\0';

    /* Act/assert: the short name is copied whole and nothing past the field is written. */
    indyrescmdCheckFileExists(0, 0, (char *)shortName);
    assertFieldHolds(shortName, 12);

    /* Act/assert: the long name is cut to 255 characters and terminated. */
    indyrescmdCheckFileExists(0, 0, longName);
    assertFieldHolds(longName, 255);

    testAMissGetsNoAnswerFromAnAbsentHost();

    puts("Indy file-name command bound checks passed");
    return 0;
}
