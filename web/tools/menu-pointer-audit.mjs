// Actual-ROM menu pointer regression. Observe native state; never write it.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';

const [
    rom,
    folder = '../build/web-menu-pointer/game',
    origin = 'http://127.0.0.1:4185/Iron-Sights/',
] = process.argv.slice(2);
if (!rom) throw new Error('Usage: menu-pointer-audit.mjs private-rom private-output [origin]');
const output = resolve(folder);
await mkdir(output, { recursive: true });
const baseline = process.env.MIDAS_EXPECT_MENU_DRIFT === '1';
const extraSettings =
    '\n[display]\nunlock_all=1\n' +
    (process.env.MIDAS_MENU_SMART_LOCK === '0' ? '\n[input]\nsmart_mouse_lock=0\n' : '');
const map = await readFile(
    new URL('../../build/web/video/ProjectMidas.map', import.meta.url),
    'utf8',
);
const localDigest = createHash('sha256')
    .update(await readFile(new URL('../../build/web/video/ProjectMidas.wasm', import.meta.url)))
    .digest('hex');
const names = [
    'cursor_h_pos',
    'cursor_v_pos',
    'current_menu',
    'mission_difficulty_highlighted',
    's_MouseVX',
    's_MouseVY',
    's_FreeDelta',
    's_MouseLocked',
    's_PollX',
    's_PollY',
];
const expressions = names.map((name) => {
    const maybeMatch = map.match(
        new RegExp(`^\\s*([0-9a-f]+)\\s+[0-9a-f]+\\s+([0-9a-f]+)\\s+${name}\\s*$`, 'm'),
    );
    if (!maybeMatch) {
        assert.ok(name.startsWith('s_'), 'Missing required native symbol ' + name);
        return `${name}:null`;
    }
    const address = parseInt(maybeMatch[1], 16),
        bytes = parseInt(maybeMatch[2], 16);
    return `${name}:${bytes === 1 ? `HEAPU8[${address}]` : `${name.startsWith('cursor_') ? 'HEAPF32' : 'HEAP32'}[${address / 4}]`}`;
});
const response = await fetch(new URL('runtime-manifest.json', origin));
assert.ok(response.ok, 'Runtime manifest request failed');
const manifest = await response.json();
assert.equal(
    manifest.wasmSHA256,
    localDigest,
    'Native symbol observation requires this exact local runtime',
);
const { browser, kind } = await launchAuditBrowser();
const page = await browser.newPage({
    viewport: { width: 1440, height: 900 },
    deviceScaleFactor: Number(process.env.MIDAS_DPR || 1),
});
const errors = [],
    samples = [];
page.on('pageerror', (error) => errors.push(String(error)));
await page.route('**/runtime/*/ProjectMidas.mjs', async (route) => {
    const response = await route.fetch();
    let source = await response.text();
    const marker = /Module\[['"]ENV['"]\]\s*=\s*ENV;/;
    assert.ok(marker.test(source));
    source = source.replace(
        marker,
        (match) => `${match} Module["auditMenuPointer"] = () => ({${expressions.join(',')}});`,
    );
    source = source.replace(
        'export default createMidas;',
        `
export default async function(options) {
    const game = await createMidas(options);
    globalThis.menuPointerGame = game;
    const original = game.callMain;
    game.callMain = function(args) {
        const ini = game.FS.readFile('/user/midas.ini', { encoding: 'utf8' });
        game.FS.writeFile('/user/midas.ini', ini + ${JSON.stringify(extraSettings)});
        return original(args);
    };
    return game;
}`,
    );
    await route.fulfill({ response, body: source });
});
const state = () => page.evaluate(() => menuPointerGame.auditMenuPointer());
async function menu(number) {
    await expect.poll(async () => (await state()).current_menu, { timeout: 15_000 }).toBe(number);
    await page.waitForTimeout(150);
}
async function point(x, y, label = '') {
    const box = await page.locator('#canvas').boundingBox();
    const band = Math.min(box.width, (box.height * 4) / 3);
    const target = {
        x: box.x + (box.width - band) / 2 + (x * band) / 440,
        y: box.y + (y * box.height) / 330,
    };
    await page.mouse.move(target.x, target.y);
    await page.waitForTimeout(100);
    const sample = { label, requested: [x, y], observed: await state(), box };
    sample.after = await page.locator('#canvas').evaluate((canvas) => ({
        width: canvas.width,
        height: canvas.height,
        rect: canvas.getBoundingClientRect().toJSON(),
    }));
    sample.error = Math.max(
        Math.abs(sample.observed.cursor_h_pos - x),
        Math.abs(sample.observed.cursor_v_pos - y),
    );
    samples.push(sample);
    if (!baseline)
        assert.ok(
            sample.error < 1.6,
            'Native cursor does not match browser pointer: ' + JSON.stringify(sample),
        );
    return target;
}
async function click(x, y, label) {
    const target = await point(x, y, label);
    await page.mouse.click(target.x, target.y, { delay: 120 });
}
try {
    await page.goto(origin + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({
        timeout: 90_000,
    });
    await page.getByRole('button', { name: 'Play GoldenEye' }).click();
    if (await page.locator('.controls-close').isVisible())
        await page.locator('.controls-close').click();
    while (true) {
        await page.waitForFunction(
            () =>
                globalThis.menuPointerGame?._portWebGetGfxTasks() > 240 ||
                [...document.querySelectorAll('button')].some(
                    (button) =>
                        /^Resume (startup|game)$/.test(button.textContent.trim()) &&
                        button.getClientRects().length,
                ),
            undefined,
            { timeout: 120_000 },
        );
        if (await page.evaluate(() => menuPointerGame._portWebGetGfxTasks() > 240)) break;
        const startup = page.getByRole('button', { name: 'Resume startup', exact: true });
        await (
            (await startup.isVisible())
                ? startup
                : page.getByRole('button', { name: 'Resume game', exact: true })
        ).click();
    }
    for (let step = 0; step < 10 && (await state()).current_menu !== 5; step++) {
        await page.keyboard.press('Enter', { delay: 120 });
        await page.waitForTimeout(1800);
    }
    await menu(5);
    for (const [x, y] of [
        [80, 40],
        [90, 50],
        [350, 200],
        [60, 260],
        [160, 100],
    ])
        await point(x, y, 'Files');
    await page.screenshot({ path: resolve(output, 'files.png') });
    if (baseline) {
        assert.ok(
            samples.some((sample) => sample.error > 20),
            'Expected to reproduce the reported drift',
        );
        console.log(
            'Baseline reproduced: ' +
                JSON.stringify(
                    samples.map((sample) => ({
                        expected: sample.requested,
                        cursor: [sample.observed.cursor_h_pos, sample.observed.cursor_v_pos],
                        freeDelta: sample.observed.s_FreeDelta,
                        error: sample.error,
                    })),
                ),
        );
    } else {
        await click(160, 100, 'Select campaign without edge sweep');
        await menu(6);
        const back = await point(220, 230, 'Right-click back');
        await page.mouse.click(back.x, back.y, { button: 'right', delay: 120 });
        await menu(5);
        await click(160, 100, 'Select campaign after right-click back');
        await menu(6);
        for (const [x, y] of [
            [40, 40],
            [400, 300],
            [220, 230],
        ])
            await point(x, y, 'Mode');
        await click(220, 230, 'Single player');
        await menu(7);
        for (const [x, y] of [
            [73, 62],
            [352, 62],
            [73, 270],
            [352, 270],
            [352, 62],
        ])
            await point(x, y, 'Mission grid');
        assert.equal(
            (await state()).mission_difficulty_highlighted,
            4,
            'Rightmost mission did not highlight',
        );
        await click(352, 62, 'Rightmost mission');
        await menu(8);
        for (const [x, y] of [
            [40, 40],
            [400, 300],
            [220, 225],
        ])
            await point(x, y, 'Difficulty');
        await click(400, 270, 'Previous tab');
        await menu(7);
        await click(73, 62, 'Dam after native menu snap');
        await menu(8);
        await click(220, 190, 'Agent');
        await menu(10);
        for (const size of [
            { width: 1200, height: 720 },
            { width: 1100, height: 1000 },
        ]) {
            await page.setViewportSize(size);
            await page.waitForTimeout(300);
            for (const [x, y] of [
                [30, 30],
                [410, 300],
                [220, 160],
            ])
                await point(x, y, 'Resized briefing');
        }
        await page.mouse.move(1, 1);
        await point(300, 220, 'Reenter canvas');
        await page.keyboard.press('F2');
        await page.keyboard.press('F2');
        if (await page.getByRole('button', { name: 'Resume game', exact: true }).isVisible())
            await page.getByRole('button', { name: 'Resume game', exact: true }).click();
        // Leaving F2 changes the canvas container; let its framebuffer/CSS fit paint.
        await page.waitForTimeout(300);
        await point(100, 70, 'Resume without edge sweep');
        await page.keyboard.press('Alt+Enter');
        await page.waitForFunction(() => !!document.fullscreenElement);
        await page.waitForTimeout(300);
        await point(390, 290, 'Fullscreen briefing');
        await page.keyboard.press('Alt+Enter');
        await page.waitForFunction(() => !document.fullscreenElement);
        await page.waitForTimeout(300);
        await point(90, 90, 'Exit fullscreen');
        assert.equal(await page.evaluate(() => document.pointerLockElement), null);
        assert.deepEqual(errors, []);
        await page.screenshot({ path: resolve(output, 'briefing.png') });
        console.log(
            `${kind} menu pointer: ${samples.length} exact absolute samples, right-click back, Files/mode/mission/difficulty/briefing clicks, resize, reentry, focus and fullscreen passed without edge sweeps`,
        );
    }
} catch (error) {
    await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
    throw error;
} finally {
    await writeFile(
        resolve(output, 'pointer.json'),
        JSON.stringify(
            {
                baseline,
                browser: process.env.MIDAS_BROWSER || kind,
                runtime: manifest.version,
                build: manifest.build,
                samples,
                errors,
            },
            null,
            2,
        ),
    );
    await browser.close();
}
