// Private real-ROM check: native HUD slider input, live preview and saved settings.
import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';

const [
    rom,
    folder = '../build/crosshair-size/browser',
    origin = 'http://127.0.0.1:4179/Iron-Sights/',
] = process.argv.slice(2);
if (!rom) throw new Error('Usage: crosshair-size-audit.mjs private-rom private-output [origin]');
const output = resolve(folder);
await mkdir(output, { recursive: true });
// Observe symbols from this local build; never write native state to steer input.
const map = await readFile(
    new URL('../../build/web/video/ProjectMidas.map', import.meta.url),
    'utf8',
);
const cursorAddresses = ['cursor_h_pos', 'cursor_v_pos'].map((name) => {
    const maybeMatch = map.match(
        new RegExp(`^\\s*([0-9a-f]+)\\s+[0-9a-f]+\\s+4\\s+${name}\\s*$`, 'm'),
    );
    assert.ok(maybeMatch, 'Missing cursor symbol: ' + name);
    return Number.parseInt(maybeMatch[1], 16);
});
const { browser } = await launchAuditBrowser();
const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
const errors = [];
page.on('pageerror', (error) => errors.push(String(error)));
await page.route('**/runtime/*/ProjectMidas.mjs', async (route) => {
    const response = await route.fetch();
    let source = await response.text();
    assert.ok(source.includes('export default createMidas;'));
    assert.ok(source.includes('var ENV = {};'));
    assert.ok(source.includes('Module["ENV"] = ENV;'));
    source = source.replace(
        'Module["ENV"] = ENV;',
        'Module["ENV"] = ENV; Module["auditCursor"] = () => [' +
            cursorAddresses.map((address) => `HEAPF32[${address / 4}]`).join(',') +
            '];',
    );
    source = source.replace('var ENV = {};', "var ENV = { GE_MENU: 'hud,900' };");
    await route.fulfill({
        response,
        body: source.replace(
            'export default createMidas;',
            `
export default async function(options) {
    globalThis.crosshairAuditLog = [];
    for (const key of ['print', 'printErr']) {
        const originalPrint = options[key];
        options[key] = function(line) { crosshairAuditLog.push(line); originalPrint(line); };
    }
    const game = await createMidas(options);
    globalThis.crosshairAuditGame = game;
    const original = game.callMain;
    game.callMain = function(args) {
        const ini = game.FS.readFile('/user/midas.ini', { encoding: 'utf8' });
        if (!ini.includes('always_crosshair_size='))
            game.FS.writeFile('/user/midas.ini', '[display]\\nalways_crosshair=1\\nalways_crosshair_size=100\\nsetting_help=0\\nmaxfps=60\\n');
        return original(args);
    };
    return game;
}`,
        ),
    });
});
const readSize = () =>
    page.evaluate(() => {
        const ini = crosshairAuditGame.FS.readFile('/user/midas.ini', { encoding: 'utf8' });
        return Number(ini.match(/^always_crosshair_size=(\d+)$/m)?.[1]);
    });
async function size(expected) {
    await expect.poll(readSize, { timeout: 10000 }).toBe(expected);
}
try {
    await page.goto(origin + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({
        timeout: 90000,
    });
    await page.getByRole('button', { name: 'Play GoldenEye' }).click();
    await page.waitForFunction(
        () => globalThis.crosshairAuditLog?.some((line) => line.includes('GE_MENU harness:')),
        undefined,
        { timeout: 120000 },
    );
    const readyTasks = await page.evaluate(() => crosshairAuditGame._portWebGetGfxTasks());
    await page.waitForFunction(
        (tasks) => crosshairAuditGame._portWebGetGfxTasks() > tasks + 120,
        readyTasks,
        { timeout: 30000 },
    );
    await page.keyboard.press('F2');
    if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
        await page.getByRole('button', { name: 'Resume game' }).click();
    await page.waitForTimeout(500);
    await size(100);
    const bounds = await page.locator('#canvas').boundingBox();
    assert.ok(bounds);
    // Browsers cannot warp the OS pointer. Observe the virtual cursor and drive
    // the real mouse by the remaining delta, rather than assuming it is absolute.
    const pointer = { x: bounds.x + bounds.width / 2, y: bounds.y + bounds.height / 2 };
    await page.mouse.move(pointer.x, pointer.y);
    await page.waitForTimeout(200);
    // The stock frontend uses a centred 440x330 canvas; help is off in this fixture.
    const scale = bounds.height / 330;
    for (let attempt = 0; attempt < 5; attempt++) {
        const current = await page.evaluate(() => {
            const [x, y] = crosshairAuditGame.auditCursor();
            return { x, y };
        });
        assert.ok(Number.isFinite(current.x) && Number.isFinite(current.y));
        if (Math.abs(current.x - 286) < 1 && Math.abs(current.y - 100) < 1) break;
        pointer.x += (286 - current.x) * scale;
        pointer.y += (100 - current.y) * scale;
        await page.mouse.move(pointer.x, pointer.y);
        await page.waitForTimeout(150);
    }
    const point = (x) => ({ x: pointer.x + (x - 286) * scale, y: pointer.y });
    await page.screenshot({ path: resolve(output, 'default.png') });
    await page.mouse.down();
    await page.waitForTimeout(100);
    const maximum = point(304);
    await page.mouse.move(maximum.x, maximum.y, { steps: 20 });
    await page.waitForTimeout(120);
    await page.mouse.up();
    await size(200);
    await page.screenshot({ path: resolve(output, 'maximum.png') });
    await page.mouse.down();
    await page.waitForTimeout(100);
    const minimum = point(270);
    await page.mouse.move(minimum.x, minimum.y, { steps: 20 });
    await page.waitForTimeout(120);
    await page.mouse.up();
    await size(10);
    await page.screenshot({ path: resolve(output, 'minimum.png') });
    // Shared option-step behavior is covered by native regressions. This audit
    // deliberately exercises actual pointer dragging and browser persistence.
    const savedSize = await readSize();
    await page.keyboard.press('F2');
    const pending = page.waitForEvent('download');
    await page.getByRole('button', { name: 'Download save backup' }).click();
    const download = await pending;
    const path = resolve(output, 'saves.json');
    await download.saveAs(path);
    const backup = JSON.parse(await readFile(path, 'utf8'));
    assert.equal(backup.format, 'project-midas-saves');
    const maybeIni = backup.files.find((file) => file.path === 'midas.ini');
    assert.ok(maybeIni);
    assert.match(
        Buffer.from(maybeIni.data, 'base64').toString(),
        new RegExp('^always_crosshair_size=' + savedSize + '$', 'm'),
    );
    await page.goto(origin); // normal remembered-ROM entry, with the debug controls closed
    await page.waitForFunction(
        () => globalThis.crosshairAuditGame?._portWebGetGfxTasks() > 60,
        undefined,
        { timeout: 120000 },
    );
    await size(savedSize);
    assert.deepEqual(errors, []);
    await writeFile(
        resolve(output, 'report.json'),
        JSON.stringify(
            {
                sizes: [100, 200, 10],
                restored: savedSize,
                errors,
            },
            null,
            2,
        ),
    );
    console.log('Crosshair: pointer drag, endpoint sizes, backup and remembered reload passed.');
} catch (error) {
    await page.screenshot({ path: resolve(output, 'failure.png') });
    await writeFile(
        resolve(output, 'failure.json'),
        JSON.stringify(
            await page.evaluate(() => ({
                log: globalThis.crosshairAuditLog,
                env: globalThis.crosshairAuditGame?.ENV,
                ini: globalThis.crosshairAuditGame?.FS.readFile('/user/midas.ini', {
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
