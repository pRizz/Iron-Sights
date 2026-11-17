/* The background converter's test fixture: a bg file's front matter built
 * here from numbers written here, the output room and the log capture, shared
 * by test_romconv_bg.c and test_romconv_bg_checks.c. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "port_romconv_stage.h"
#include "test_romconv_bg_fixture.h"

char s_Log[512];
u8 s_Fm[FM_SIZE];
union BgOut s_Out;
bg_room_data *s_Rooms;
bg_portal_data_entry *s_Portals;

void portConLogf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s_Log, sizeof(s_Log), fmt, ap);
    va_end(ap);
}

int startsWith(const char *text, const char *prefix)
{
    while (*prefix != '\0' && *text == *prefix)
    {
        text++;
        prefix++;
    }
    return *prefix == '\0';
}

int sameBytes(const void *a, const void *b, size_t size)
{
    const u8 *x = (const u8 *) a;
    const u8 *y = (const u8 *) b;
    size_t i;

    for (i = 0; i < size; i++)
    {
        if (x[i] != y[i])
        {
            return 0;
        }
    }
    return 1;
}

int expect(int ok, const char *what)
{
    if (!ok)
    {
        printf("test_romconv_bg: FAIL: %s\n", what);
        return 1;
    }
    return 0;
}

void put32(u32 at, u32 value)
{
    memcpy(s_Fm + at, &value, sizeof(value));
}

static void putFloat(u32 at, f32 value)
{
    memcpy(s_Fm + at, &value, sizeof(value));
}

static void putRoom(u32 room, u32 point, u32 primary, u32 secondary, f32 x, f32 y, f32 z)
{
    u32 at = FM_ROOMS + room * 24;

    put32(at, point);
    put32(at + 4, primary);
    put32(at + 8, secondary);
    putFloat(at + 12, x);
    putFloat(at + 16, y);
    putFloat(at + 20, z);
}

static void putPortal(u32 portal, u32 offset, u8 room1, u8 room2, u8 cb1, u8 cb2)
{
    u32 at = FM_PORTALS + portal * 8;

    put32(at, offset);
    s_Fm[at + 4] = room1;
    s_Fm[at + 5] = room2;
    s_Fm[at + 6] = cb1;
    s_Fm[at + 7] = cb2;
}

void buildFrontMatter(void)
{
    u32 i;

    memset(s_Fm, 0, sizeof(s_Fm));
    put32(0x04, 0x0F000000u | FM_ROOMS);
    put32(0x08, 0x0F000000u | FM_PORTALS);
    put32(0x0C, 0x0F000000u | FM_VIS);
    putRoom(1, 0x0F000200u, 0x0F000280u, 0x0F000300u, 1.5f, 12.5f, -3.25f);
    putRoom(2, 0x0F000400u, 0x0F000500u, 0, 100.0f, 0.0f, 7.0f);
    putRoom(3, 0x0F000900u, 0x0F000980u, 0x0F000990u, 0.0f, 0.0f, 0.0f);
    put32(FM_VIS, 0x05080000u);
    put32(FM_VIS + 4, 0x0F0000A4u);
    putPortal(0, 0x0F000000u | FM_POINTS0, 3, 4, 0x01, 0x02);
    putPortal(1, 0x0F000000u | FM_POINTS1, 1, 2, 0x80, 0x40);
    s_Fm[FM_POINTS0] = 4;
    s_Fm[FM_POINTS1] = 3;
    for (i = 0; i < 4 * 3; i++)
    {
        putFloat(FM_POINTS0 + 4 + i * 4, (f32) i);
    }
    for (i = 0; i < 3 * 3; i++)
    {
        putFloat(FM_POINTS1 + 4 + i * 4, -(f32) i);
    }
}

void freshOut(void)
{
    memset(s_Out.bytes, FILL, sizeof(s_Out.bytes));
    s_Rooms = NULL;
    s_Portals = NULL;
}

int outUntouched(u32 from)
{
    u32 i;

    for (i = from; i < sizeof(s_Out.bytes); i++)
    {
        if (s_Out.bytes[i] != FILL)
        {
            return 0;
        }
    }
    return 1;
}

int convertStock(void)
{
    buildFrontMatter();
    freshOut();
    return portBgTablesConvert(s_Fm, FM_SIZE, s_Out.bytes, portBgTablesSize(s_Fm, FM_SIZE, "test"), &s_Rooms,
            &s_Portals);
}

int expectRefused(u32 size, u32 outsize, const char *why, const char *what)
{
    u32 n;
    int result;
    char measured[sizeof(s_Log)];

    s_Log[0] = '\0';
    n = portBgTablesSize(s_Fm, size, "test");
    memcpy(measured, s_Log, sizeof(measured));
    freshOut();
    s_Log[0] = '\0';
    result = portBgTablesConvert(s_Fm, size, s_Out.bytes, outsize, &s_Rooms, &s_Portals);
    return expect(n == 0 && startsWith(measured, REFUSAL) && startsWith(measured + sizeof(REFUSAL) - 1, why)
            && result == 0 && outUntouched(0) && s_Rooms == NULL && s_Portals == NULL
            && startsWith(s_Log, CONVERT_REFUSAL), what);
}
