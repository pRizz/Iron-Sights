// Optional read-only watch observation against the matching local Wasm linker map.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { expect } from '@playwright/test';

export async function auditTouchWatch(page, context) {
    const maybeMap = process.env.MIDAS_TOUCH_WATCH_MAP;
    if (!maybeMap) return;
    const map = await readFile(maybeMap, 'utf8');
    const address = (name) => {
        const match = map.match(new RegExp(`^\\s*([0-9a-f]+)\\s+\\S+\\s+4\\s+${name}$`, 'm'));
        assert.ok(match, `Missing local watch symbol: ${name}`);
        return parseInt(match[1], 16);
    };
    const tab = address('watch_screen_index');
    const briefing = address('mission_brief_index');
    const read = (at) => page.evaluate((pointer) => midasAuditReadU32(pointer), at);
    const session = await context.newCDPSession(page);
    const holdStep = async (direction, at, expected) => {
        const box = await page.locator(`[data-touch-action=${direction}]`).boundingBox();
        await session.send('Input.dispatchTouchEvent', {
            type: 'touchStart',
            touchPoints: [{ id: 1, x: box.x + box.width / 2, y: box.y + box.height / 2 }],
        });
        try {
            await expect.poll(() => read(at)).toBe(expected);
            await page.waitForTimeout(200);
            assert.equal(await read(at), expected, `${direction} skipped an item`);
            await page.waitForTimeout(1000);
            assert.equal(await read(at), expected, `${direction} repeated while held`);
        } finally {
            await session.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
        }
        await page.waitForTimeout(150);
    };
    const initialTab = await read(tab);
    assert.equal(initialTab, 0, 'Expected the initial Mission Status watch page');
    await holdStep('right', tab, 1);
    await holdStep('left', tab, 0);
    await holdStep('left', tab, 4);
    const initialBriefing = await read(briefing);
    await holdStep('down', briefing, (initialBriefing + 1) % 5);
    await holdStep('up', briefing, initialBriefing);
    console.log(
        'Watch: all four touch arrows move exactly one item and remain stable during holds',
    );
}
