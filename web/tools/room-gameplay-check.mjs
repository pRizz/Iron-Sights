// Private normal-menu Dam/watch/focus check after entering through the 3D room.
import assert from 'node:assert/strict';
import { resolve } from 'node:path';
import { readAudio } from './audio-observer.mjs';

export async function checkRoomGameplay(page, context, output) {
    for (let step = 0; step < 7; step++) {
        if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
            await page.getByRole('button', { name: 'Resume game' }).click();
        await page.keyboard.press('Enter', { delay: 120 });
        await page.waitForTimeout(1800);
    }
    await page.evaluate(() => document.exitPointerLock());
    // Ensure the game is paused even when the current native screen is a menu.
    await page.getByRole('button', { name: 'Pause', exact: true }).click();
    await page.getByRole('button', { name: 'Resume game' }).click();
    await page.waitForTimeout(300);
    if (
        !(await page.locator('.player-toolbar').getByText('Playing', { exact: true }).isVisible())
    ) {
        await page.keyboard.press('p', { delay: 120 });
        await page.getByRole('button', { name: 'Resume game' }).click({ timeout: 60_000 });
    }
    await page.waitForFunction(() => !!document.pointerLockElement);
    await page.keyboard.press('w', { delay: 250 });
    await page.screenshot({ path: resolve(output, 'dam.png') });
    const audio = await page.evaluate(readAudio);
    await page.keyboard.press('p', { delay: 120 });
    await page.waitForFunction(() => !document.pointerLockElement);
    await page.waitForTimeout(1000);
    assert.ok(
        !(await page.getByRole('button', { name: 'Resume game' }).isVisible()),
        'Watch mistaken for focus loss',
    );
    await page.screenshot({ path: resolve(output, 'watch.png') });
    await page.keyboard.press('p', { delay: 120 });
    await page.getByRole('button', { name: 'Resume game' }).click();
    await page.waitForFunction(() => !!document.pointerLockElement);
    const other = await context.newPage();
    await other.goto('about:blank');
    await other.bringToFront();
    await page.getByRole('button', { name: 'Resume game' }).waitFor();
    await page.waitForTimeout(1200);
    const paused = await page.locator('pre').textContent();
    await page.waitForTimeout(1200);
    assert.equal(
        await page.locator('pre').textContent(),
        paused,
        'Simulation advanced without focus',
    );
    await page.bringToFront();
    await page.getByRole('button', { name: 'Resume game' }).click();
    await page.waitForFunction(() => !!document.pointerLockElement);
    await other.close();
    await page.evaluate(() => document.exitPointerLock());
    await page.getByRole('button', { name: 'Resume game' }).waitFor();
    return audio;
}
