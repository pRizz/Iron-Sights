/* ROM-free checks of the simulation trace (LP64-006 Task 3,
 * test_lp64_compare.sh): one line a tick from the game's state, with floats as
 * their bits, counts and hashes over values, and no address. port_simtrace.c
 * is included whole; this file defines the game state it reads and fabricates
 * one tick of it. The scenarios that read GE_SIMTRACE or GE_SIMTRACE_DETAIL
 * run in their own child, since port_simtrace.c reads them once a process. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "port_simtrace.h"
#include "../../port/src/port_simtrace.c"

/* The game state port_simtrace.c reads. */
s32 g_GlobalTimer;
u64 g_randomSeed;
s32 g_portRandDrawCount;
struct player *g_playerPointers[4];
ChrRecord *g_ChrSlots;
s32 g_NumChrSlots;
PropRecord g_Props[MAX_PROPS];
PropRecord *g_ActivePropsHead;
s_room_info g_BgRoomInfo[MAXROOMCOUNT];
s32 g_MaxNumRooms;

#define TEST_CHR_SLOTS 4
#define TEST_ROOMS 8

/* The line of the fabricated tick, call 1 with 2 draws. Its hashes were
 * computed apart from port_simtrace.c, by FNV-1a over the values the header
 * lists, each four bytes from the lowest. */
static const char kFabricatedLine[] = "T n=1 timer=1 seed=00000000deadbeef draws=2 players=1 "
                                      "p0=3f800000,00000000,c2c80000,40490fdb,3f800000 "
                                      "chrs=3:0a1b2466 props=3:f375b044 rooms=2:f5ead2c0\n";

static Model *const kSomeModel = (Model *) &g_MaxNumRooms; /* only tested against NULL */
static int failures;

static void expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_simtrace: FAIL (%d-bit): %s\n", (int) (sizeof(void *) * 8), what);
        failures++;
    }
}

static void setPos(coord3d *pos, f32 x, f32 y, f32 z)
{
    pos->x = x;
    pos->y = y;
    pos->z = z;
}

static void setChr(ChrRecord *chr, PropRecord *prop, u8 action, u32 flags, f32 x, f32 y, f32 z, f32 damage)
{
    chr->model = kSomeModel;
    chr->prop = prop;
    chr->actiontype = action;
    chr->chrflags = (CHRFLAG) flags;
    chr->damage = damage;
    setPos(&prop->pos, x, y, z);
}

static void setProp(PropRecord *prop, PropRecord *prev, u8 type, u8 flags, f32 x, f32 y, f32 z)
{
    prop->type = type;
    prop->flags = flags;
    prop->next = NULL;
    setPos(&prop->pos, x, y, z);
    if (prev != NULL)
    {
        prev->next = prop;
    }
}

/* The fabricated tick: one player, three guards in four slots, three active
 * props from g_Props starting at FIRSTPROP, two loaded rooms. Each call gives
 * fresh heap records, so two calls hold the same state at other addresses. */
static void fabricate(s32 firstProp)
{
    struct player *player = calloc(1, sizeof(*player));
    PropRecord *playerProp = calloc(1, sizeof(*playerProp));
    PropRecord *a = &g_Props[firstProp];
    PropRecord *b = &g_Props[firstProp + 1];
    PropRecord *c = &g_Props[firstProp + 2];

    memset(g_playerPointers, 0, sizeof(g_playerPointers));
    memset(g_Props, 0, sizeof(g_Props));
    memset(g_BgRoomInfo, 0, sizeof(g_BgRoomInfo));
    g_GlobalTimer = 1;
    g_randomSeed = 0xdeadbeefull;
    setPos(&playerProp->pos, 1.0f, 0.0f, -100.0f);
    player->prop = playerProp;
    player->vv_theta = 3.14159265f;
    player->bondhealth = 1.0f;
    g_playerPointers[0] = player;
    g_NumChrSlots = TEST_CHR_SLOTS;
    g_ChrSlots = calloc(TEST_CHR_SLOTS, sizeof(ChrRecord));
    setChr(&g_ChrSlots[0], b, 1, 0x10, 10.0f, 0.0f, 20.0f, 0.0f);
    setChr(&g_ChrSlots[1], calloc(1, sizeof(PropRecord)), 2, 0, -5.5f, 1.0f, 2.0f, 0.5f);
    setChr(&g_ChrSlots[3], calloc(1, sizeof(PropRecord)), 3, 0x80000000u, 0.0f, 0.0f, 0.0f, 1.0f);
    setProp(a, NULL, 1, 0x03, 100.0f, 0.0f, -50.0f);
    setProp(b, a, 3, 0x01, 10.0f, 0.0f, 20.0f);
    setProp(c, b, 4, 0x05, 0.25f, -0.25f, 8.0f);
    g_ActivePropsHead = a;
    g_MaxNumRooms = TEST_ROOMS;
    g_BgRoomInfo[2].model_bin_loaded = 1;
    g_BgRoomInfo[5].model_bin_loaded = 3;
}

static void format(char *line, size_t size)
{
    if (portSimTraceFormat(line, size, 1, 2) < 0)
    {
        line[0] = '\0';
    }
}

/* The text of FIELD's value in LINE ("chrs" gives "3:0a1b2466"). */
static const char *fieldOf(const char *line, const char *field, char *out, size_t size)
{
    char key[32];
    const char *at;
    size_t length;

    snprintf(key, sizeof(key), " %s=", field);
    at = strstr(line, key);
    if (at == NULL)
    {
        return "";
    }
    at += strlen(key);
    length = strcspn(at, " \n");
    snprintf(out, size, "%.*s", (int) (length < size ? length : size - 1), at);
    return out;
}

static void lineHasEveryFieldInOrder(void)
{
    char line[PORT_SIMTRACE_LINE_MAX];

    // Arrange
    fabricate(0);

    // Act
    format(line, sizeof(line));

    // Assert
    expect(strcmp(line, kFabricatedLine) == 0, "line_has_every_field_in_order");
    if (strcmp(line, kFabricatedLine) != 0)
    {
        printf("  got:      %s  expected: %s", line, kFabricatedLine);
    }
}

static void floatIsWrittenAsBits(void)
{
    char line[PORT_SIMTRACE_LINE_MAX];
    char p0[64];

    // Arrange
    fabricate(0);
    g_playerPointers[0]->bondhealth = 0.1f;

    // Act
    format(line, sizeof(line));

    // Assert
    expect(strcmp(fieldOf(line, "p0", p0, sizeof(p0)), "3f800000,00000000,c2c80000,40490fdb,3dcccccd") == 0,
           "float_is_written_as_bits");
}

static void oneBitInOneGuardChangesTheChrHash(void)
{
    char before[PORT_SIMTRACE_LINE_MAX];
    char after[PORT_SIMTRACE_LINE_MAX];
    char chrsBefore[32];
    char chrsAfter[32];
    char propsBefore[32];
    char propsAfter[32];
    u32 bits;

    // Arrange
    fabricate(0);
    format(before, sizeof(before));
    memcpy(&bits, &g_ChrSlots[1].damage, sizeof(bits));
    bits ^= 1u;
    memcpy(&g_ChrSlots[1].damage, &bits, sizeof(bits));

    // Act
    format(after, sizeof(after));

    // Assert
    fieldOf(before, "chrs", chrsBefore, sizeof(chrsBefore));
    fieldOf(after, "chrs", chrsAfter, sizeof(chrsAfter));
    fieldOf(before, "props", propsBefore, sizeof(propsBefore));
    fieldOf(after, "props", propsAfter, sizeof(propsAfter));
    expect(strcmp(chrsBefore, chrsAfter) != 0 && strcmp(propsBefore, propsAfter) == 0,
           "one_bit_in_one_guard_changes_the_chr_hash");
}

static void noPointerReachesTheFile(void)
{
    char first[PORT_SIMTRACE_LINE_MAX];
    char second[PORT_SIMTRACE_LINE_MAX];
    ChrRecord *firstSlots;

    // Arrange: the second state's heap records are fresh (the first's are
    // never freed) and its props sit in other g_Props slots.
    fabricate(0);
    format(first, sizeof(first));
    firstSlots = g_ChrSlots;
    fabricate(100);

    // Act
    format(second, sizeof(second));

    // Assert
    expect(firstSlots != g_ChrSlots && strcmp(first, second) == 0, "no_pointer_reaches_the_file");
}

/* Counts LINES' lines that start with KIND and hold " n=N ". */
static int countLines(const char *lines, char kind, u32 n)
{
    char key[32];
    const char *line = lines;
    int count = 0;

    snprintf(key, sizeof(key), " n=%u ", (unsigned) n);
    while (*line != '\0')
    {
        const char *end = strchr(line, '\n');
        const size_t length = end != NULL ? (size_t) (end - line) : strlen(line);
        char copy[PORT_SIMTRACE_LINE_MAX];

        snprintf(copy, sizeof(copy), "%.*s ", (int) length, line);
        if (copy[0] == kind && strstr(copy, key) != NULL)
        {
            count++;
        }
        line += length + (end != NULL);
    }
    return count;
}

static int readFile(const char *path, char *out, size_t size)
{
    FILE *file = fopen(path, "rb");
    size_t got;

    if (file == NULL)
    {
        return 0;
    }
    got = fread(out, 1, size - 1, file);
    out[got] = '\0';
    fclose(file);
    return 1;
}

/* Child: four ticks with details for calls 2 to 3; prop a moves every tick. */
static int detailRangeIsInclusive(void)
{
    char path[] = "/tmp/midas-simtrace.XXXXXX";
    static char lines[16384];
    const int fd = mkstemp(path);
    u32 n;
    int ok = 1;

    // Arrange
    if (fd < 0)
    {
        perror("mkstemp");
        return 1;
    }
    close(fd);
    setenv("GE_SIMTRACE", path, 1);
    setenv("GE_SIMTRACE_DETAIL", "2-3", 1);
    fabricate(0);

    // Act
    for (n = 1; n <= 4; n++)
    {
        g_Props[0].pos.x += 1.0f;
        portSimTraceTick();
    }

    // Assert
    ok = readFile(path, lines, sizeof(lines));
    unlink(path);
    for (n = 1; n <= 4; n++)
    {
        const int detailed = (n == 2 || n == 3);

        ok = ok && countLines(lines, 'T', n) == 1;
        ok = ok && countLines(lines, 'G', n) == (detailed ? 3 : 0);
        ok = ok && countLines(lines, 'M', n) == (detailed ? 1 : 0);
    }
    if (!ok)
    {
        printf("  trace:\n%s", lines);
    }
    return ok ? 0 : 1;
}

/* Child: a trace file that cannot be opened stops the run. */
static int unopenableFileStopsTheRun(void)
{
    if (freopen("/dev/null", "w", stderr) == NULL) /* keeps the expected FATAL line out of the output */
    {
        return 2;
    }
    setenv("GE_SIMTRACE", "/nonexistent-midas-dir/trace.txt", 1);
    unsetenv("GE_SIMTRACE_DETAIL");
    fabricate(0);
    portSimTraceTick();
    return 0; /* reached only if the failure was swallowed */
}

/* Runs SCENARIO in a child and expects its exit status WANT. */
static void inChild(const char *name, int (*scenario)(void), int want)
{
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    if (child < 0)
    {
        perror("fork");
        failures++;
        return;
    }
    if (child == 0)
    {
        status = scenario();
        fflush(NULL); /* _exit skips stdio */
        _exit(status);
    }
    expect(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == want, name);
}

int main(void)
{
    lineHasEveryFieldInOrder();
    floatIsWrittenAsBits();
    oneBitInOneGuardChangesTheChrHash();
    noPointerReachesTheFile();
    inChild("detail_range_is_inclusive", detailRangeIsInclusive, 0);
    inChild("unopenable_file_stops_the_run", unopenableFileStopsTheRun, 1);
    if (failures)
    {
        return 1;
    }
    printf("test_simtrace: PASS (%d-bit)\n", (int) (sizeof(void *) * 8));
    return 0;
}
