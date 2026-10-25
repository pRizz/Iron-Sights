// Private normal-menu Dam/watch/focus check after entering through the 3D room.
import assert from 'node:assert/strict';
import { resolve } from 'node:path';
import { readAudio } from './audio-observer.mjs';
import { checkPresentation } from './presentation-check.mjs';
import { navigateToDam, captureGameplay } from './native-game-audit.mjs';

export async function checkRoomGameplay(page, context, output) {
    await navigateToDam(page);
    await captureGameplay(page);
    await page.keyboard.press('w', { delay: 250 });
    await page.screenshot({ path: resolve(output, 'dam.png') });
    const audio = await page.evaluate(readAudio);
    await page.keyboard.press('F2');
    await checkPresentation(page);
    await page.screenshot({ path: resolve(output, 'dam-viewport.png') });
    await page.keyboard.press('F2');
    await page.getByRole('button', { name: 'Resume game' }).click();
    await page.waitForFunction(() => !!document.pointerLockElement);
    await page.keyboard.press('p', { delay: 120 });
    await page.waitForFunction(() => !document.pointerLockElement);
    await page.waitForTimeout(1000);
    assert.ok(
        !(await page.getByRole('button', { name: 'Resume game' }).isVisible()),
        'Watch mistaken for focus loss',
    );
    await page.screenshot({ path: resolve(output, 'watch.png') });
    await page.keyboard.press('p', { delay: 120 });
    await page.waitForFunction(
        () => !!document.pointerLockElement || !!document.querySelector('.mouse-capture-hint'),
    );
    assert.ok(
        !(await page.getByRole('button', { name: 'Resume game' }).isVisible()),
        'Closing the watch incorrectly paused gameplay',
    );
    if (!(await page.evaluate(() => !!document.pointerLockElement)))
        await page.locator('.mouse-capture-hint').click();
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
