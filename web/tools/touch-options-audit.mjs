// Private real-ROM check: every Options folder state that a menu tap cannot
// answer has working touch controls - a long pick list scrolls and closes, a
// bind row's capture cancels and unbinds, Input Device's wait cancels, and the
// Files screen's Erase prompt answers Cancel or Confirm by tap.
import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';

const [
    rom,
    folder = '../build/web-touch/options',
    origin = 'http://127.0.0.1:4177/Iron-Sights/',
    mapPath = '../build/web/video/ProjectMidas.map',
] = process.argv.slice(2);
if (!rom)
    throw new Error('Usage: touch-options-audit.mjs private-rom [private-output] [origin] [map]');
const output = resolve(folder);
await mkdir(output, { recursive: true });
const map = await readFile(resolve(mapPath), 'utf8');
const symbol = (name) => {
    const match = map.match(new RegExp(`^\\s*([0-9a-f]+)\\s+\\S+\\s+\\d+\\s+${name}$`, 'm'));
    assert.ok(match, `Missing symbol in the local map: ${name}`);
    return parseInt(match[1], 16);
};
// Native 440x330 menu positions of the rows under test; unset rows only save
// a screenshot of their sheet for locating them.
const rows = {
    bind: Number(process.env.MIDAS_BIND_Y || 0),
    device: Number(process.env.MIDAS_DEVICE_Y || 0),
    list: Number(process.env.MIDAS_LIST_Y || 0),
};
const rowX = Number(process.env.MIDAS_ROW_X || 120);
const { browser } = await launchAuditBrowser();
const errors = [];
const report = {};

async function boot(menu, name) {
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
    page.on('pageerror', (error) => errors.push(`${name}: ${error}`));
    await page.route('**/runtime/*/ProjectMidas.mjs', async (route) => {
        const response = await route.fetch();
        let source = await response.text();
        for (const marker of [
            'export default createMidas;',
            'var ENV = {};',
            'function getMemoryBuffer() {',
        ])
            assert.ok(source.includes(marker), marker);
        source = source.replace('var ENV = {};', `var ENV = { GE_MENU: '${menu}' };`);
        source = source.replace(
            'function getMemoryBuffer() {',
            `function getMemoryBuffer() {
    globalThis.optionsAuditMemory = () => wasmMemory.buffer;`,
        );
        await route.fulfill({
            response,
            body: source.replace(
                'export default createMidas;',
                `
export default async function(options) {
    globalThis.optionsAuditLog = [];
    for (const key of ['print', 'printErr']) {
        const originalPrint = options[key];
        options[key] = function(line) { optionsAuditLog.push(line); originalPrint(line); };
    }
    const game = await createMidas(options);
    globalThis.optionsAuditGame = game;
    const original = game.callMain;
    game.callMain = function(args) {
        game.FS.writeFile('/user/midas.ini', '[display]\\nsetting_help=0\\nmaxfps=60\\n');
        return original(args);
    };
    return game;
}`,
            ),
        });
    });
    await page.goto(origin + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    const play = page.getByRole('button', { name: 'Play GoldenEye' });
    await expect(play).toBeEnabled({ timeout: 90000 });
    await play.tap();
    await page.waitForFunction(
        () => globalThis.optionsAuditLog?.some((line) => line.includes('GE_MENU harness:')),
        undefined,
        { timeout: 120000 },
    );
    const tasks = await page.evaluate(() => optionsAuditGame._portWebGetGfxTasks());
    await page.waitForFunction(
        (ready) => optionsAuditGame._portWebGetGfxTasks() > ready + 120,
        tasks,
        { timeout: 30000 },
    );
    const overlay = page.locator('.touch-controls');
    await expect(overlay).toHaveAttribute('data-context', 'navigation');
    const bounds = await page.locator('#canvas').boundingBox();
    assert.ok(bounds);
    const band = Math.min(bounds.width, (bounds.height * 4) / 3);
    const screen = (x, y) => ({
        x: bounds.x + (bounds.width - band) / 2 + (x * band) / 440,
        y: bounds.y + (y * bounds.height) / 330,
    });
    // A real finger rests for a few frames; Playwright's tap has no hold time.
    const fingerTap = async ({ x, y }) => {
        await page.locator('.touch-menu-surface').evaluate(
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
    const read32 = (address) =>
        page.evaluate((at) => new Int32Array(optionsAuditMemory())[at >>> 2], address);
    const write32 = (address, value) =>
        page.evaluate(
            ([at, next]) => {
                new Int32Array(optionsAuditMemory())[at >>> 2] = next;
            },
            [address, value],
        );
    const read8 = (address) =>
        page.evaluate((at) => new Uint8Array(optionsAuditMemory())[at], address);
    const write8 = (address, value) =>
        page.evaluate(
            ([at, next]) => {
                new Uint8Array(optionsAuditMemory())[at] = next;
            },
            [address, value],
        );
    const ini = () =>
        page.evaluate(() => optionsAuditGame.FS.readFile('/user/midas.ini', { encoding: 'utf8' }));
    const shot = (file) => page.screenshot({ path: resolve(output, `${name}-${file}.png`) });
    return { context, page, overlay, screen, fingerTap, read32, write32, read8, write8, ini, shot };
}

async function scenario(menu, name, run) {
    const session = await boot(menu, name);
    try {
        await session.shot('sheet');
        await run(session);
    } catch (error) {
        await session.shot('failure');
        await writeFile(
            resolve(output, `${name}-failure.json`),
            JSON.stringify(
                await session.page.evaluate(() => globalThis.optionsAuditLog?.slice(-40)),
                null,
                2,
            ),
        );
        throw error;
    } finally {
        await session.context.close();
    }
}

try {
    // Video > Max FPS: 19 entries in a 13-row list.
    await scenario(
        'video,900',
        'list',
        async ({ page, overlay, screen, fingerTap, read32, shot }) => {
            if (!rows.list) return console.log('Set MIDAS_LIST_Y from list-sheet.png');
            const scroll = symbol('port_sheet_list_scroll');
            const listRow = symbol('port_sheet_list_row');
            await fingerTap(screen(rowX, rows.list));
            await expect(overlay).toHaveAttribute('data-context', 'list');
            await shot('open');
            const opened = await read32(scroll);
            await page.locator('[data-touch-step="3"]').tap();
            await page.locator('[data-touch-step="3"]').tap();
            await page.locator('[data-touch-step="3"]').tap();
            await expect.poll(() => read32(scroll)).toBeGreaterThan(opened);
            const bottom = await read32(scroll);
            await page.locator('[data-touch-step="3"]').tap();
            await page.waitForTimeout(200);
            assert.equal(await read32(scroll), bottom, 'scrolling stops at the last row');
            await page.locator('[data-touch-step="-3"]').tap();
            await expect.poll(() => read32(scroll)).toBe(Math.max(0, bottom - 3));
            await shot('scrolled');
            await page.locator('[data-touch-action=back]').tap();
            await expect(overlay).toHaveAttribute('data-context', 'navigation');
            await expect.poll(() => read32(listRow)).toBe(-1);
            report.list = { opened, bottom };
        },
    );

    // A keyboard/mouse bind row (the browser's default device).
    await scenario('binds,900', 'bind', async ({ page, overlay, screen, fingerTap, shot }) => {
        if (!rows.bind) return console.log('Set MIDAS_BIND_Y from bind-sheet.png');
        const row = screen(rowX, rows.bind);
        // The row's bind column. Keyboard binds reach midas.ini only when the
        // page saves, so the drawn value is what shows the unbind.
        const from = screen(190, rows.bind - 7);
        const to = screen(300, rows.bind + 7);
        const cell = () =>
            page.screenshot({
                clip: { x: from.x, y: from.y, width: to.x - from.x, height: to.y - from.y },
            });
        await fingerTap(row);
        await expect(overlay).toHaveAttribute('data-context', 'capture');
        await expect(page.locator('.touch-menu-surface')).toHaveCount(0);
        await shot('capture');
        await page.locator('[data-touch-action=back]').tap();
        await expect(overlay).toHaveAttribute('data-context', 'navigation');
        await page.waitForTimeout(400);
        const bound = await cell();
        // Cancel stayed on the page and kept the bind: the same row arms again.
        await fingerTap(row);
        await expect(overlay).toHaveAttribute('data-context', 'capture');
        await page.locator('[data-touch-request=unbind]').tap();
        await expect(overlay).toHaveAttribute('data-context', 'navigation');
        await page.waitForTimeout(400);
        assert.ok(!bound.equals(await cell()), 'Unbind changed the drawn bind');
        await shot('unbound');
        report.bind = 'Cancel kept the bind; Unbind cleared it';
    });

    // Player 1's Input Device row waits for a press on a real device.
    await scenario('player1,900', 'device', async ({ page, overlay, screen, fingerTap, shot }) => {
        if (!rows.device) return console.log('Set MIDAS_DEVICE_Y from device-sheet.png');
        await fingerTap(screen(rowX, rows.device));
        await expect(overlay).toHaveAttribute('data-context', 'assign');
        await shot('waiting');
        const started = Date.now();
        await page.locator('[data-touch-action=back]').tap();
        await expect(overlay).toHaveAttribute('data-context', 'navigation', { timeout: 3000 });
        report.device = { cancelledAfterMs: Date.now() - started };
    });

    // The Files screen's Erase prompt, armed directly: it needs a folder with
    // Dam on Agent finished, which a fresh browser profile does not have.
    await scenario(
        'select,900',
        'erase',
        async ({ overlay, screen, fingerTap, read32, write32, read8, write8, shot }) => {
            const selected = symbol('folder_selected_for_deletion');
            const choice = symbol('folder_selected_for_deletion_choice');
            // The optimizer keeps the flag as one byte and splits the box array
            // into scalars named port_erase_box.N.
            const valid = symbol('port_erase_box_valid');
            const box = (n) => symbol(`port_erase_box\.${n}`);
            const arm = async () => {
                await write8(valid, 0);
                await write32(choice, 1);
                await write32(selected, 0);
                await expect.poll(() => read8(valid)).toBe(1);
            };
            const centre = async (k) => {
                const values = [];
                for (let i = 0; i < 4; i++) values.push(await read32(box(k * 4 + i)));
                return screen((values[0] + values[2]) / 2, (values[1] + values[3]) / 2);
            };
            await expect(overlay).toHaveAttribute('data-context', 'navigation');
            // A tap away from both answers keeps the default, Cancel.
            await arm();
            await shot('prompt');
            await fingerTap(screen(400, 40));
            await expect.poll(() => read32(selected)).toBe(-1);
            assert.equal(await read32(choice), 1, 'a stray tap must not erase');
            // A tap on Confirm answers Confirm; one on Cancel answers Cancel.
            await arm();
            await fingerTap(await centre(0));
            await expect.poll(() => read32(selected)).toBe(-1);
            assert.equal(await read32(choice), 0, 'a tap on Confirm confirms');
            await arm();
            await write32(choice, 0);
            await fingerTap(await centre(1));
            await expect.poll(() => read32(selected)).toBe(-1);
            assert.equal(await read32(choice), 1, 'a tap on Cancel cancels');
            report.erase = 'stray tap kept Cancel; Confirm and Cancel taps answered';
        },
    );
    assert.deepEqual(errors, []);
    await writeFile(resolve(output, 'report.json'), JSON.stringify(report, null, 2));
    console.log(
        'Touch Options: list scroll/close, bind cancel/unbind, device-wait cancel and Erase taps passed.',
    );
} finally {
    await browser.close();
}
