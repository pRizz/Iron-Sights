/* Included by test_hud.c: the top-left stack (clock, subtitles, objective
 * alerts) and the shared corner layout (FORK-015). */

static void testObjectiveSubtitleClearance(void)
{
    /* Arrange: shifted viewports, regional metrics, one to six subtitle rows. */
    const s32 heights[] = {8, 11, 14, 20};
    s32 origin, scaled, row, font, best;
    for (origin = 0; origin <= 12; origin += 12)
    for (scaled = 0; scaled <= 1; scaled++)
    for (row = 1; row <= 6; row++)
    for (font = 0; font < 4; font++)
    for (best = 0; best <= 1; best++) {
        const s32 subtitleBottom = origin + (13 + row * heights[font] + scaled) / (scaled ? 2 : 1);
        const PortMissionClockLayout clock = portMissionClockLayout(0, origin, heights[font], best);
        /* Act: use the subtitle renderer's measured and scaled bottom edge. */
        const s32 y = portHudObjectiveLayoutTop(origin, clock.toastY, subtitleBottom, 0);
        /* Assert: the complete panel clears text, its bar and the optional clock. */
        assert(y >= origin + 28 && y >= clock.toastY && y >= subtitleBottom + 4);
        assert(y + 30 <= origin + 180);
        assert(portHudObjectiveLayoutTop(origin, 0, subtitleBottom, 0) >= subtitleBottom + 4);
    }
    assert(portHudObjectiveLayoutTop(12, 0, 0, 0) == 40); /* no displayed subtitle */
    assert(portHudObjectiveLayoutTop(0, 0, 20, 0) == 28); /* short line needs no move */
    assert(portHudObjectiveLayoutTop(0, 0, 46, 0) == 50); /* Janus's three-line dialogue */
}

static void testTopLeftCornerNoOverlap(void)
{
    /* Arrange: FPS widths from 2 to 5 digits, regional metrics, safe-area
     * insets of a landscape iPhone (24 units) and none, shifted viewports,
     * full-size and halved HUDs, with and without a saved best time. */
    const s32 heights[] = {8, 11, 14, 20};
    const s32 widths[] = {14, 21, 28, 35};
    s32 origin, small, font, w, safeX, safeY, best;
    for (origin = 0; origin <= 12; origin += 12)
    for (small = 0; small <= 1; small++)
    for (font = 0; font < 4; font++)
    for (w = 0; w < 4; w++)
    for (safeX = 0; safeX <= 24; safeX += 24)
    for (safeY = 0; safeY <= 12; safeY += 12)
    for (best = 0; best <= 1; best++) {
        const s32 scale = small ? 2 : 1, height = heights[font];
        /* Act: place the counter, then the clock, subtitle, card and debug rows. */
        const PortHudBox fps = portHudFpsLayout(0, origin, safeX, safeY, widths[w], height);
        const s32 offset = portHudClockOffsetX(0, fps.right, safeX, scale);
        const PortMissionClockLayout clock = portMissionClockLayout(offset, origin + safeY * scale, height, best);
        const s32 clockLeft = (clock.x) / scale;              /* screen units, pivot at x 0 */
        const s32 clockBottom = origin + (clock.bottom - origin + scale - 1) / scale;
        const s32 subtitleX = 0x1e, subtitleLeft = subtitleX / scale;
        const s32 fpsBlocks = fps.right + 2 > subtitleLeft ? fps.bottom : 0;
        const s32 y = portHudSubtitleLayoutTop(origin, origin + 13, clock.bottom, small, fpsBlocks);
        const s32 barTop = origin + (y - 2 - origin) / scale;
        const s32 barBottom = origin + (y + 2 * height - origin + scale - 1) / scale;
        const s32 card = portHudObjectiveLayoutTop(origin, clock.toastY, barBottom, fps.bottom);
        const s32 debug = portHudDebugLayoutTop(origin + 0x11, fps.bottom, clockBottom, 0);
        /* Assert: the counter sits inside the safe area, the clock starts four
         * units after it on the same row, and every lower element clears both. */
        assert(fps.x >= 3 + safeX && fps.y >= origin + 3 + safeY);
        assert(clockLeft >= fps.right + 4);
        assert(clockLeft >= safeX);
        assert(!fpsBlocks || barTop >= fps.bottom + 4);
        assert(barTop >= clockBottom + 4);
        assert(card >= barBottom + 4 && card >= fps.bottom + 4 && card >= clock.toastY);
        assert(debug >= fps.bottom + 4 && debug >= clockBottom + 4 && debug >= origin + 0x11);
    }
    /* Nothing moves on a desktop with a two- or three-digit counter. */
    {
        const PortHudBox fps = portHudFpsLayout(0, 0, 0, 0, 21, 8);
        assert(portHudClockOffsetX(0, fps.right, 0, 1) == 0);
        assert(portHudClockOffsetX(0, 0, 0, 1) == 0);
        assert(portHudSubtitleLayoutTop(0, 13, 0, 0, 0) == 13);
        assert(portHudDebugLayoutTop(0x11, fps.bottom, 0, 0) == 0x11);
        assert(portHudObjectiveLayoutTop(0, 0, 0, fps.bottom) == 28);
    }
    /* A NOCLIP label at the top right pushes the debug rows below it. */
    assert(portHudDebugLayoutTop(0x11, 0, 0, 19) == 23);
}
