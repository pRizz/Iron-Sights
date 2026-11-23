/* A campaign save at either width (LP64-006 Task 13, test_save_widths.sh).
 * file2.c's save code and libultra_shim.c's EEPROM are included whole, as the
 * typed-read tests include game files, with port_lockstep.c, which the shim's
 * clock calls; crc.c and n64_random.c, the save checksum, are compiled beside
 * this file. --gc-sections drops what the save path does not reach, so only
 * that needs a stand-in here.
 *
 *   test_save_widths write DIR   saves the campaign below into DIR/Data/Saves/eeprom.bin
 *   test_save_widths read DIR    loads that file and prints every field
 *   test_save_widths padding DIR checks that a stored folder record's padding is 0 (64 bits)
 *
 * Both start as the game's boot does, with fileValidateSaves; write refuses a
 * folder that already holds a save, so the campaign is always built on a blank
 * cart. The game's own writers make it, so the bytes are what the game would
 * store. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-function-declaration"
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
#include "../../src/game/file2.c"
#pragma GCC diagnostic pop
#include "../../port/src/libultra_shim.c"
#include "../../port/src/port_lockstep.c"

/* ---- the campaign ------------------------------------------------------------------------------ */
#define AGENT_MISSIONS 12
#define AGENT_SECONDS(level) (100 + 37 * (level))
#define OO_MISSIONS 3
#define OO_SECONDS(level) (600 + 50 * (level))
static const s32 kEarnedCheats[] = {3, 11};
/* The volumes the folder's options hold when it is saved, as the sound code reports them. */
#define MUSIC_RAW 0x3000
#define SFX_RAW 0x5800
#define FOLDER3_DAM_SECONDS 321

/* ---- the save folder --------------------------------------------------------------------------- */
static char sRoot[1024];

const char *portRootPath(const char *name)
{
    static char path[1200];

    snprintf(path, sizeof(path), "%s/%s", sRoot, name);
    return path;
}

FILE *portRootFopen(const char *name, const char *mode)
{
    return fopen(portRootPath(name), mode);
}

/* The shim's log lines (`ge: eeprom.bin loaded`) go to stderr; stdout holds only the fields. */
void portConLogf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

/* ---- what the save path reaches outside these files -------------------------------------------- */
/* joy.c's pak calls without its controller-poll pause, which the shim does not need. */
OSMesgQueue g_ContInputMessageQueue;

s32 joyGamePakProbe(void)
{
    return osEepromProbe(&g_ContInputMessageQueue);
}

s32 joyGamePakLongRead(u8 address, u8 *buffer, s32 nbytes)
{
    return osEepromLongRead(&g_ContInputMessageQueue, address, buffer, nbytes);
}

s32 joyGamePakLongWrite(u8 address, u8 *buffer, s32 nbytes)
{
    return osEepromLongWrite(&g_ContInputMessageQueue, address, buffer, nbytes);
}

s32 get_debug_enable_agent_levels_flag(void)
{
    return 0;
}
s32 get_debug_enable_all_levels_flag(void)
{
    return 0;
}
void portHeadshotsFolderErased(s32 folder)
{
    (void) folder;
}
/* fileSaveSettingsForFolder's sources: the two volumes, the default options, control style 1.1.
 * Unlock All (portUnlockAll) and the volume adoption are the shim's weak stand-ins. */
u16 get_mTrack2Vol(void)
{
    return MUSIC_RAW;
}
u16 call_sndGetSfxSlotFirstNaturalVolume(void)
{
    return SFX_RAW;
}
u32 get_cur_player_look_vertical_inverted(void)
{
    return 0;
}
s32 cur_player_get_autoaim(void)
{
    return 1;
}
u32 cur_player_get_aim_control(void)
{
    return 0;
}
u32 cur_player_get_sight_onscreen_control(void)
{
    return 1;
}
u32 cur_player_get_lookahead(void)
{
    return 1;
}
u32 cur_player_get_ammo_onscreen_setting(void)
{
    return 1;
}
u32 cur_player_get_screen_setting(void)
{
    return SCREEN_SIZE_FULLSCREEN;
}
SCREEN_RATIO_OPTION get_screen_ratio(void)
{
    return SCREEN_RATIO_NORMAL;
}
static struct player sPlayer;
struct player *g_CurrentPlayer = &sPlayer;

/* ---- write ------------------------------------------------------------------------------------- */
static int writeCampaign(void)
{
    char save[1200];
    struct stat st;
    s32 level;
    size_t i;

    snprintf(save, sizeof(save), "%s", portRootPath(PORT_EEPROM_FILE));
    if (stat(save, &st) == 0)
    {
        fprintf(stderr, "test_save_widths: %s exists; write needs a folder without a save\n", save);
        return 1;
    }
    fileValidateSaves();
    for (level = SP_LEVEL_DAM; level < AGENT_MISSIONS; level++)
    {
        fileUnlockStageInFolderAtDifficulty(FOLDER1, level, DIFFICULTY_AGENT, AGENT_SECONDS(level));
    }
    for (level = SP_LEVEL_DAM; level < OO_MISSIONS; level++)
    {
        fileUnlockStageInFolderAtDifficulty(FOLDER1, level, DIFFICULTY_00, OO_SECONDS(level));
    }
    for (i = 0; i < sizeof(kEarnedCheats) / sizeof(kEarnedCheats[0]); i++)
    {
        fileSaveFolderUnlockCheat(FOLDER1, kEarnedCheats[i]);
    }
    fileClearSavefileForFolder(FOLDER1); /* the Options folder's save: the volumes above */
    fileUnlockStageInFolderAtDifficulty(FOLDER3, SP_LEVEL_DAM, DIFFICULTY_AGENT, FOLDER3_DAM_SECONDS);
    if (stat(save, &st) != 0 || st.st_size != PORT_EEPROM_SIZE)
    {
        fprintf(stderr, "test_save_widths: write left no %d-byte %s\n", PORT_EEPROM_SIZE, save);
        return 1;
    }
    return 0;
}

/* ---- read -------------------------------------------------------------------------------------- */
static void printSlot(s32 slot)
{
    const save_data *save = &saves[slot];

    if (fileGetSaveFlagDoReset(&saves[slot]))
    {
        printf("slot %d: free\n", slot);
        return;
    }
    printf("slot %d: flags 0x%02x flag_007 %u music %u sfx %u options 0x%04x cheats 0x%02x 0x%02x 0x%02x padding %d\n",
           slot, save->completion_bitflags, save->flag_007, save->music_vol, save->sfx_vol, save->options,
           save->unlocked_cheats_1, save->unlocked_cheats_2, save->unlocked_cheats_3, save->padding);
}

static void printFolder(s32 folder)
{
    static const char *const names[] = {"agent", "secret", "00"};
    save_data *save = fileGetSaveForFoldernum(folder);
    s32 difficulty, level, cheat, earned = 0;

    if (save == NULL)
    {
        printf("folder %d: none\n", folder + 1);
        return;
    }
    printf("folder %d: slot %d rotation %u bond %u 007 %d highest %d earned", folder + 1, (s32) (save - saves),
           fileGetSaveFlagSlot(save), fileGetSelectedBond(save), fileIs007ModeUnlocked(folder),
           fileGetHighestStageUnlockedForFolder(folder));
    for (cheat = 0; cheat < CHEAT_INPUT_BUFFER_SIZE; cheat++)
    {
        if (portFileGetIsCheatUnlockedRaw(save, cheat))
        {
            printf(" %d", cheat);
            earned++;
        }
    }
    printf("%s\n", earned ? "" : " none");
    for (difficulty = DIFFICULTY_AGENT; difficulty < DIFFICULTY_007; difficulty++)
    {
        printf("folder %d %s:", folder + 1, names[difficulty]);
        for (level = SP_LEVEL_DAM; level < SP_LEVEL_MAX; level++)
        {
            printf(" %d", fileGetSaveStageDifficultyTime(save, level, difficulty));
        }
        printf("\n");
    }
}

static int readCampaign(void)
{
    smallSave header;
    s32 i;

    fileValidateSaves();
    joyGamePakLongRead(0, (u8 *) &header, sizeof(header));
    printf("header 0x%02x\n", header.unk[0]);
    for (i = SAVESLOT1; i < SAVESLOTRAMROM; i++)
    {
        printSlot(i);
    }
    for (i = FOLDER1; i < MAX_FOLDER_COUNT; i++)
    {
        printFolder(i);
    }
    return 0;
}

/* ---- padding ----------------------------------------------------------------------------------- */
/* A folder record ends in padding after its times, which the checksum covers. At 64 bits
 * fileWriteSave stores it as zeros whatever the record in memory holds there. */
static int writeClearsPadding(void)
{
    u8 file[PORT_EEPROM_SIZE];
    u8 *tail = saves[SAVESLOT1].times + sizeof(saves[SAVESLOT1].times);
    const size_t padding = (u8 *) &saves[SAVESLOT1 + 1] - tail;
    const size_t at = sizeof(smallSave) + (size_t) (tail - (u8 *) saves);
    size_t i;

    // Arrange
    fileValidateSaves();
    memset(tail, 0xa5, padding);

    // Act
    fileWriteSave(&saves[SAVESLOT1]);

    // Assert
    osEepromLongRead(&g_ContInputMessageQueue, 0, file, sizeof(file));
    for (i = 0; i < padding; i++)
    {
        if (file[at + i] != 0)
        {
            fprintf(stderr, "test_save_widths: the stored record's padding byte %zu is 0x%02x, not 0\n", i, file[at + i]);
            return 1;
        }
    }
    printf("padding: %zu bytes stored as 0\n", padding);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3 || (strcmp(argv[1], "write") != 0 && strcmp(argv[1], "read") != 0 && strcmp(argv[1], "padding") != 0))
    {
        fprintf(stderr, "usage: test_save_widths write|read|padding DIR\n");
        return 2;
    }
    snprintf(sRoot, sizeof(sRoot), "%s", argv[2]);
    if (strcmp(argv[1], "padding") == 0)
    {
        return writeClearsPadding();
    }
    return strcmp(argv[1], "write") == 0 ? writeCampaign() : readCampaign();
}
