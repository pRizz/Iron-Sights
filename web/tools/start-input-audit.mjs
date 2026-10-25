// Private real-ROM acceptance: web Enter is Start, including the native watch.
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { installGameProbe, navigateToDam, captureGameplay } from './native-game-audit.mjs';

const [
    rom,
    folder = '../build/web-start-input/game',
    origin = 'http://127.0.0.1:4177/Project-Midas-ADS/',
] = process.argv.slice(2);
if (!rom) throw new Error('Usage: start-input-audit.mjs private-rom private-output [origin]');
const output = resolve(folder);
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
await installGameProbe(page);
const errors = [];
page.on('pageerror', (error) => errors.push(String(error)));
const live = async (value) =>
    page.waitForFunction(
        (value) => Boolean(midasAuditGame._portWebControlsLive()) === value,
        value,
        { timeout: 10000 },
    );
const press = async (key) => page.keyboard.press(key, { delay: 120 });
try {
    await page.goto(origin + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({
        timeout: 90000,
    });
    await page.getByRole('button', { name: 'Play GoldenEye' }).click();
    await page.waitForFunction(
        () => globalThis.midasAuditGame?._portWebGetGfxTasks() >= 240,
        undefined,
        { timeout: 120000 },
    );
    await page.locator('.controls-close').click();
    if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
        await page.getByRole('button', { name: 'Resume game' }).click();
    await navigateToDam(page);
    await captureGameplay(page);
    await press('Enter');
    await live(false);
    await page.waitForTimeout(700);
    await press('Enter');
    await live(true);
    await captureGameplay(page);
    await press('P');
    await live(false);
    await page.waitForTimeout(700);
    await press('Enter');
    await live(true);
    await captureGameplay(page);
    await page.keyboard.down('Enter');
    await live(false);
    await page.waitForTimeout(700);
    assert.equal(
        await page.evaluate(() => Boolean(midasAuditGame._portWebControlsLive())),
        false,
        'Held Start retriggered the watch',
    );
    await page.keyboard.up('Enter');
    await page.waitForTimeout(200);
    await press('P');
    await live(true);
    await captureGameplay(page);
    await press('Alt+Enter');
    await page.waitForFunction(() => !!document.fullscreenElement);
    await live(true);
    await press('Alt+Enter');
    await page.waitForFunction(() => !document.fullscreenElement);
    await live(true);
    await expect(page.locator('.pause-overlay')).toHaveCount(0);
    await page.evaluate(() => document.exitPointerLock());
    const resume = page.getByRole('button', { name: 'Resume game' });
    await expect(resume).toBeVisible();
    await resume.focus();
    await press('Enter');
    await page.waitForFunction(() => !!document.pointerLockElement);
    await page.waitForTimeout(500);
    await live(true);
    await expect(page.locator('.pause-overlay')).toHaveCount(0);
    assert.deepEqual(errors, []);
    const report = {
        browser: browser.version(),
        checks: 'Enter menus/watch, P interoperability, held Start edge, Alt+Enter isolation, Enter activates HTML Resume without spilling into watch',
        errors,
    };
    await writeFile(resolve(output, 'report.json'), JSON.stringify(report, null, 2));
    console.log(JSON.stringify(report));
} catch (error) {
    await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
    await writeFile(resolve(output, 'failure.txt'), String(error));
    await writeFile(
        resolve(output, 'native-log.txt'),
        await page
            .locator('pre')
            .textContent()
            .catch(() => ''),
    );
    throw error;
} finally {
    await browser.close();
}
