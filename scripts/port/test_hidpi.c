/* Host tests for port/src/port_hidpi.c (MAC-002 Task 6): points to pixels
 * and back, and a screenshot's rows repacked for a BMP file at any width. No
 * game header: the functions use only <stdint.h>. Each test prints its name
 * and aborts on a failed check. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_hidpi.h"

#define CHECK(cond) \
    do \
    { \
        if (!(cond)) \
        { \
            fprintf(stderr, "test_hidpi: FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            abort(); \
        } \
    } while (0)

static void aPointScalesToItsPixel(void)
{
    puts("a point scales to its pixel");
    /* Act and assert: 640 points drawn on 1,280 pixels */
    CHECK(portHidpiToPixels(100, 640, 1280) == 200);
    CHECK(portHidpiToPixels(0, 640, 1280) == 0);
}

static void theLastPointMapsInsideTheLastPixelPair(void)
{
    int32_t pixel;

    puts("the last point maps inside the last pixel pair");
    /* Act */
    pixel = portHidpiToPixels(639, 640, 1280);
    /* Assert */
    CHECK(pixel == 1278 || pixel == 1279);
}

static void equalSizesReturnTheValue(void)
{
    puts("equal sizes return the value");
    /* Act and assert */
    CHECK(portHidpiToPixels(123, 640, 640) == 123);
    CHECK(portHidpiToPoints(123, 640, 640) == 123);
}

static void aWindowWithNoSizeReturnsTheValue(void)
{
    puts("a window with no size returns the value");
    /* Act and assert: before the window exists SDL reports 0 */
    CHECK(portHidpiToPixels(77, 0, 1280) == 77);
    CHECK(portHidpiToPixels(77, -1, 1280) == 77);
    CHECK(portHidpiToPoints(77, 640, 0) == 77);
    CHECK(portHidpiToPoints(77, 0, 1280) == 77);
}

static void aScaleThatIsNotWholeRoundsToNearest(void)
{
    puts("a scale that is not whole rounds to nearest");
    /* Act and assert: 1.5 pixels a point */
    CHECK(portHidpiToPixels(1, 1000, 1500) == 2);
    CHECK(portHidpiToPixels(3, 1000, 1500) == 5);
    CHECK(portHidpiToPoints(5, 1000, 1500) == 3);
}

static void aPositionLeftOfTheWindowStaysNegative(void)
{
    puts("a position left of the window stays negative");
    /* Act and assert: SDL reports a captured pointer outside the window */
    CHECK(portHidpiToPixels(-10, 640, 1280) == -20);
    CHECK(portHidpiToPoints(-20, 640, 1280) == -10);
}

static void toPointsOfToPixelsReturnsThePoint(void)
{
    int32_t point;

    puts("to-points of to-pixels returns the point");
    /* Act and assert: every coordinate of this Mac's 1,728-point row */
    for (point = 0; point < 1728; point++)
    {
        CHECK(portHidpiToPoints(portHidpiToPixels(point, 1728, 3456), 1728, 3456) == point);
    }
}

/* The three bytes glReadPixels would give pixel X of row Y. */
static uint8_t sourceByte(int x, int y, int channel)
{
    return (uint8_t) (x * 7 + y * 31 + channel * 101 + 5);
}

/* Packs HEIGHT rows of WIDTH pixels and checks every byte of the result. */
static void checkPackedRows(int width, int height)
{
    size_t stride = (size_t) width * 3;
    size_t padded = (stride + 3) & ~(size_t) 3;
    /* One guard byte past the image: an overrun inside the buffer shows. */
    uint8_t *pixels = malloc(padded * (size_t) height + 1);
    int x;
    int y;
    size_t pad;

    CHECK(pixels != NULL);
    /* Arrange: packed RGB rows, as glReadPixels returns them with a pack
     * alignment of 1, and stale bytes behind them. */
    memset(pixels, 0xEE, padded * (size_t) height + 1);
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pixels[(size_t) y * stride + (size_t) x * 3 + 0] = sourceByte(x, y, 0);
            pixels[(size_t) y * stride + (size_t) x * 3 + 1] = sourceByte(x, y, 1);
            pixels[(size_t) y * stride + (size_t) x * 3 + 2] = sourceByte(x, y, 2);
        }
    }
    /* Act */
    portBmpPackRows(pixels, width, height);
    /* Assert: each row at its padded offset, blue first, its padding zero */
    for (y = 0; y < height; y++)
    {
        const uint8_t *row = pixels + (size_t) y * padded;

        for (x = 0; x < width; x++)
        {
            CHECK(row[(size_t) x * 3 + 0] == sourceByte(x, y, 2));
            CHECK(row[(size_t) x * 3 + 1] == sourceByte(x, y, 1));
            CHECK(row[(size_t) x * 3 + 2] == sourceByte(x, y, 0));
        }
        for (pad = stride; pad < padded; pad++)
        {
            CHECK(row[pad] == 0);
        }
    }
    CHECK(pixels[padded * (size_t) height] == 0xEE);
    free(pixels);
}

static void rowsArePackedAtEveryWidth(void)
{
    /* 1 to 4 cover each padding; 4,096 was the old fixed row's limit, and
     * 4,097 and 5,120 (a 5K display) overran it. */
    static const int widths[] = { 1, 2, 3, 4, 4096, 4097, 5120 };
    size_t i;

    puts("rows are packed at every width");
    for (i = 0; i < sizeof(widths) / sizeof(widths[0]); i++)
    {
        checkPackedRows(widths[i], 2);
    }
}

static void anEmptyImageIsLeftAlone(void)
{
    uint8_t pixels[4] = { 1, 2, 3, 4 };

    puts("an empty image is left alone");
    /* Act */
    portBmpPackRows(pixels, 0, 2);
    portBmpPackRows(pixels, 1, 0);
    /* Assert */
    CHECK(pixels[0] == 1 && pixels[1] == 2 && pixels[2] == 3 && pixels[3] == 4);
}

int main(void)
{
    rowsArePackedAtEveryWidth();
    anEmptyImageIsLeftAlone();
    aPointScalesToItsPixel();
    theLastPointMapsInsideTheLastPixelPair();
    equalSizesReturnTheValue();
    aWindowWithNoSizeReturnsTheValue();
    aScaleThatIsNotWholeRoundsToNearest();
    aPositionLeftOfTheWindowStaysNegative();
    toPointsOfToPixelsReturnsThePoint();
    return 0;
}
