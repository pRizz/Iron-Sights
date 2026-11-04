// Private real-ROM check: a touch user can edit an Options folder slider
// (Audio > Music Volume) with -/+, a sideways drag, Done and Cancel.
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';

const [rom, folder = '../build/web-touch/slider', origin = 'http://127.0.0.1:4177/Iron-Sights/'] =
    process.argv.slice(2);
if (!rom) throw new Error('Usage: touch-slider-audit.mjs private-rom private-output [origin]');
const output = resolve(folder);
await mkdir(output, { recursive: true });
// Native 440x330 menu position of the Music Volume row on the Audio sheet.
const musicRow = {
    x: Number(process.env.MIDAS_SLIDER_X || 120),
    y: Number(process.env.MIDAS_SLIDER_Y || 0),
};
const { browser } = await launchAuditBrowser();
const context = await browser.newContext({
    viewport: { width: 844, height: 390 },
    deviceScaleFactor: 2,
    isMobile: true,
    hasTouch: true,
});
await context.addInitScript(() => {
    delete HTMLCanvasElement.prototype.requestPointerLock;
});
const page = await context.newPage();
const errors = [];
page.on('pageerror', (error) => errors.push(String(error)));
await page.route('**/runtime/*/ProjectMidas.mjs', async (route) => {
    const response = await route.fetch();
    let source = await response.text();
    assert.ok(source.includes('export default createMidas;'));
    assert.ok(source.includes('var ENV = {};'));
    source = source.replace('var ENV = {};', "var ENV = { GE_MENU: 'audio,900' };");
    await route.fulfill({
        response,
        body: source.replace(
            'export default createMidas;',
            `
export default async function(options) {
    globalThis.sliderAuditLog = [];
    for (const key of ['print', 'printErr']) {
        const originalPrint = options[key];
        options[key] = function(line) { sliderAuditLog.push(line); originalPrint(line); };
    }
    const game = await createMidas(options);
    globalThis.sliderAuditGame = game;
    const original = game.callMain;
    game.callMain = function(args) {
        game.FS.writeFile('/user/midas.ini', '[display]\\nsetting_help=0\\nmaxfps=60\\nmusic_volume=50\\n');
        return original(args);
    };
    return game;
}`,
        ),
    });
});
const readMusic = () =>
    page.evaluate(() => {
        const ini = sliderAuditGame.FS.readFile('/user/midas.ini', { encoding: 'utf8' });
        return Number(ini.match(/^music_volume=(\d+)$/m)?.[1]);
    });
const music = (expected) => expect.poll(readMusic, { timeout: 10000 }).toBe(expected);
const overlay = page.locator('.touch-controls');
const shot = (name) => page.screenshot({ path: resolve(output, name + '.png') });
try {
    await page.goto(origin + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({
        timeout: 90000,
    });
    await page.getByRole('button', { name: 'Play GoldenEye' }).tap();
    await page.waitForFunction(
        () => globalThis.sliderAuditLog?.some((line) => line.includes('GE_MENU harness:')),
        undefined,
        { timeout: 120000 },
    );
    const readyTasks = await page.evaluate(() => sliderAuditGame._portWebGetGfxTasks());
    await page.waitForFunction(
        (tasks) => sliderAuditGame._portWebGetGfxTasks() > tasks + 120,
        readyTasks,
        { timeout: 30000 },
    );
    await expect(overlay).toHaveAttribute('data-context', 'navigation');
    await shot('sheet');
    if (!musicRow.y) {
        console.log('Set MIDAS_SLIDER_Y to the Music Volume row from sheet.png');
        process.exit(0);
    }
    const bounds = await page.locator('#canvas').boundingBox();
    assert.ok(bounds);
    const band = Math.min(bounds.width, (bounds.height * 4) / 3);
    const screen = (x, y) => ({
        x: bounds.x + (bounds.width - band) / 2 + (x * band) / 440,
        y: bounds.y + (y * bounds.height) / 330,
    });
    const row = screen(musicRow.x, musicRow.y);
    const surface = page.locator('.touch-menu-surface');
    // A real finger rests for a few frames; Playwright's tap has no hold time,
    // so its cursor move and A would land in the same native tick.
    const fingerTap = async (x, y) => {
        await surface.evaluate(
            (element, [px, py]) =>
                element.dispatchEvent(
                    new PointerEvent('pointerdown', {
                        pointerId: 76,
                        pointerType: 'touch',
                        bubbles: true,
                        cancelable: true,
                        clientX: px,
                        clientY: py,
                    }),
                ),
            [x, y],
        );
        await page.waitForTimeout(120);
        await page.evaluate(
            ([px, py]) =>
                document.dispatchEvent(
                    new PointerEvent('pointerup', {
                        pointerId: 76,
                        pointerType: 'touch',
                        bubbles: true,
                        clientX: px,
                        clientY: py,
                    }),
                ),
            [x, y],
        );
    };
    await fingerTap(row.x, row.y);
    await expect(overlay).toHaveAttribute('data-context', 'edit');
    await expect(page.locator('[data-touch-step]')).toHaveCount(2);
    await shot('edit-open');
    await page.locator('[data-touch-step="1"]').tap();
    await page.locator('[data-touch-step="1"]').tap();
    await page.locator('[data-touch-step="-1"]').tap();
    await page.waitForTimeout(200);
    // The edit holds ini writes until it closes; Done keeps 55.
    await page.locator('[data-touch-action=accept]').tap();
    await expect(overlay).toHaveAttribute('data-context', 'navigation');
    await music(55);

    // A long rightward drag clamps at 100 rather than wrapping to 0.
    await fingerTap(row.x, row.y);
    await expect(overlay).toHaveAttribute('data-context', 'edit');
    const drag = async (fromX, toX) => {
        const steps = 24;
        await surface.evaluate(
            (element, [x, y, count, to]) => {
                const send = (type, at) =>
                    (type === 'pointerdown' ? element : document).dispatchEvent(
                        new PointerEvent(type, {
                            pointerId: 77,
                            pointerType: 'touch',
                            bubbles: true,
                            cancelable: true,
                            clientX: at,
                            clientY: y,
                        }),
                    );
                send('pointerdown', x);
                for (let i = 1; i <= count; i++) send('pointermove', x + ((to - x) * i) / count);
                send('pointerup', to);
            },
            [fromX, 200, steps, toX],
        );
        await page.waitForTimeout(300);
    };
    await drag(150, 800);
    await shot('edit-dragged');
    await page.locator('[data-touch-action=accept]').tap();
    await music(100);

    // Cancel restores the value the edit opened with.
    await fingerTap(row.x, row.y);
    await expect(overlay).toHaveAttribute('data-context', 'edit');
    await drag(700, 300);
    await page.locator('[data-touch-action=back]').tap();
    await expect(overlay).toHaveAttribute('data-context', 'navigation');
    await page.waitForTimeout(300);
    await music(100);

    // A leftward drag also clamps, at 0.
    await fingerTap(row.x, row.y);
    await expect(overlay).toHaveAttribute('data-context', 'edit');
    await drag(800, 20);
    await drag(800, 20);
    await fingerTap(400, 200); // a plain tap keeps the edit too
    await expect(overlay).toHaveAttribute('data-context', 'navigation');
    await music(0);
    await shot('done');
    assert.deepEqual(errors, []);
    await writeFile(
        resolve(output, 'report.json'),
        JSON.stringify({ values: [55, 100, 100, 0], errors }, null, 2),
    );
    console.log('Touch slider: -/+, Done, clamped drags, Cancel and tap-to-keep passed.');
} catch (error) {
    await shot('failure');
    await writeFile(
        resolve(output, 'failure.json'),
        JSON.stringify(
            await page.evaluate(() => ({
                log: globalThis.sliderAuditLog?.slice(-40),
                ini: globalThis.sliderAuditGame?.FS.readFile('/user/midas.ini', {
                    encoding: 'utf8',
                }),
            })),
            null,
            2,
        ),
    );
    throw error;
} finally {
    await browser.close();
}
