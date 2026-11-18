/*
 * indyrescmdCheckFileExists copies a file name into its command's 256-byte
 * field with the game's own strncpy (src/str.c). That strncpy decrements its
 * count only for the characters before the terminator, so asked for the whole
 * field it writes one byte past it for any shorter name: under the sanitizer
 * that is a stack-buffer-overflow. Every build also checks the command sent.
 */
#include "src/str.c"
#include "src/game/indy_commands.c"
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

    puts("Indy file-name command bound checks passed");
    return 0;
}
