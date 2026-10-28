// Actual-ROM file-screen transfer acceptance. All data/captures stay private.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { installGameProbe } from './native-game-audit.mjs';

const [rom, directory = '../build/web-files/game', origin = 'http://127.0.0.1:4185/Iron-Sights/'] =
    process.argv.slice(2);
if (!rom) throw new Error('Usage: file-menu-save-audit.mjs local-rom [private-output] [origin]');
const output = resolve(directory);
await mkdir(output, { recursive: true });
const { browser } = await launchAuditBrowser();
const context = await browser.newContext({
    viewport: { width: 1440, height: 900 },
    hasTouch: process.env.MIDAS_TOUCH === '1',
});
const page = await context.newPage();
const errors = [];
page.on('pageerror', (error) => errors.push(String(error)));
await installGameProbe(page);
const importer = page.getByRole('button', { name: 'Import Save', exact: true });
const exporter = page.getByRole('button', { name: 'Export Save', exact: true });

async function reachFiles() {
    console.log('Waiting for initial native frames');
    while (true) {
        await page.waitForFunction(
            () =>
                globalThis.midasAuditGame?._portWebGetGfxTasks() > 240 ||
                [...document.querySelectorAll('button')].some(
                    (button) =>
                        /^Resume (startup|game)$/.test(button.textContent.trim()) &&
                        button.getClientRects().length,
                ),
            undefined,
            { timeout: 120_000 },
        );
        if (await page.evaluate(() => midasAuditGame._portWebGetGfxTasks() > 240)) break;
        const startup = page.getByRole('button', { name: 'Resume startup', exact: true });
        await (
            (await startup.isVisible())
                ? startup
                : page.getByRole('button', { name: 'Resume game', exact: true })
        ).click();
    }
    for (let step = 0; step < 10; step++) {
        console.log(
            'Menu step',
            step,
            await page.evaluate(() => ({
                frames: midasAuditGame._portWebGetGfxTasks(),
                file: midasAuditGame._portWebFileMenuMetric(0),
                stage: midasAuditGame._portWebGetStage(),
            })),
        );
        if (await importer.isVisible()) return;
        if (await page.getByRole('button', { name: 'Resume game' }).isVisible()) {
            await page.getByRole('button', { name: 'Resume game' }).click();
            await page.waitForTimeout(200);
            continue;
        }
        if (await page.evaluate(() => midasAuditGame._portWebFileMenuMetric(0))) {
            await expect(importer).toBeVisible();
            return;
        }
        await page.keyboard.press('Enter', { delay: 120 });
        await page.waitForTimeout(1800);
    }
    await expect(importer).toBeVisible();
}

async function nativeClick(button) {
    const box = await button.boundingBox();
    const element = await button.elementHandle();
    await element.evaluate((button) => {
        button.style.pointerEvents = 'none';
    });
    try {
        await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
        await page.waitForTimeout(120);
        await page.mouse.click(box.x + box.width / 2, box.y + box.height / 2, { delay: 120 });
    } finally {
        await element.evaluate((button) => {
            button.style.pointerEvents = '';
        });
    }
}

async function backup(native = false) {
    await expect(exporter).toBeVisible();
    const [download] = await Promise.all([
        page.waitForEvent('download'),
        native ? nativeClick(exporter) : exporter.click(),
    ]);
    const chunks = [];
    for await (const chunk of await download.createReadStream()) chunks.push(chunk);
    return JSON.parse(Buffer.concat(chunks).toString());
}

async function choose(value, native = false) {
    await expect(importer).toBeVisible();
    if (native) {
        await nativeClick(importer);
        await expect(
            page.getByRole('heading', { name: 'Import saves', exact: true }),
        ).toBeVisible();
    }
    const [picker] = await Promise.all([
        page.waitForEvent('filechooser'),
        native
            ? page.getByRole('button', { name: 'Choose backup file' }).click()
            : importer.click(),
    ]);
    await picker.setFiles({
        name: 'portable.json',
        mimeType: 'application/json',
        buffer: Buffer.from(JSON.stringify(value)),
    });
}

try {
    await page.goto(origin + '?controls');
    console.log('Selecting local ROM');
    await expect(importer).toHaveCount(0);
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({
        timeout: 90_000,
    });
    await page.getByRole('button', { name: 'Play GoldenEye' }).click();
    console.log('Starting game; closing HTML controls');
    if (await page.locator('.controls-close').isVisible())
        await page.locator('.controls-close').click();
    await reachFiles();
    const canvas = await page.locator('#canvas').elementHandle();
    await page.screenshot({ path: resolve(output, 'files-desktop.png') });
    const original = await backup();
    if (process.env.MIDAS_TOUCH !== '1')
        assert.deepEqual(
            (await backup(true)).files,
            original.files,
            'Native cursor export differs',
        );
    assert.equal(original.format, 'project-midas-saves');
    await choose(original, process.env.MIDAS_TOUCH !== '1');
    const dialog = page.getByRole('dialog').filter({ hasText: 'Replace local saves?' });
    await dialog.getByRole('button', { name: 'Cancel', exact: true }).click();
    assert.deepEqual((await backup()).files, original.files, 'Cancelled import changed saves');
    assert.ok(await canvas.evaluate((element) => element === document.querySelector('#canvas')));
    await choose({ ...original, version: 99 });
    await expect(page.locator('.experience-hint')).toContainText('version 1');
    assert.deepEqual((await backup()).files, original.files, 'Invalid import changed saves');
    const profile = Buffer.from('private imported profile fixture');
    const settings = Buffer.from('[display]\nalways_health=0\nmaxfps=60\n[input]\nmouse=1\n');
    const transferred = {
        ...original,
        files: original.files.filter((file) => file.path !== 'midas.ini'),
    };
    for (const [path, bytes] of [
        ['midas.ini', settings],
        ['Data/Profiles/Transfer.geprofile', profile],
    ])
        transferred.files.push({
            path,
            data: bytes.toString('base64'),
            sha256: createHash('sha256').update(bytes).digest('hex'),
        });
    await choose(transferred);
    await Promise.all([
        page.waitForNavigation({ waitUntil: 'load' }),
        dialog.getByRole('button', { name: 'Import and restart' }).click(),
    ]);
    await reachFiles();
    const restored = (await backup()).files;
    for (const path of ['Data/Saves/eeprom.bin', 'Data/Profiles/Transfer.geprofile'])
        assert.deepEqual(
            restored.find((file) => file.path === path),
            transferred.files.find((file) => file.path === path),
        );
    const ini = Buffer.from(
        restored.find((file) => file.path === 'midas.ini').data,
        'base64',
    ).toString();
    assert.match(ini, /always_health=0/, 'Import lost an explicit HUD preference');
    for (const size of [
        { width: 390, height: 844 },
        { width: 844, height: 390 },
        { width: 1024, height: 768 },
    ]) {
        await page.setViewportSize(size);
        await expect(importer).toBeVisible();
        await page.waitForTimeout(300);
        for (const button of [importer, exporter]) {
            const box = await button.boundingBox();
            assert.ok(
                box.x >= 0 &&
                    box.y >= 0 &&
                    box.x + box.width <= size.width + 1 &&
                    box.y + box.height <= size.height + 1,
            );
            if (process.env.MIDAS_TOUCH === '1') {
                const cluster = await page.locator('.touch-n64').boundingBox();
                assert.ok(
                    box.y >= cluster.y + cluster.height ||
                        box.y + box.height <= cluster.y ||
                        box.x >= cluster.x + cluster.width ||
                        box.x + box.width <= cluster.x,
                    'File actions overlap N64 controls',
                );
            }
        }
        await page.screenshot({ path: resolve(output, `files-${size.width}.png`) });
    }
    await page.keyboard.press('Enter', { delay: 120 });
    await expect(importer).toHaveCount(0, { timeout: 15_000 });
    assert.deepEqual(errors, []);
    console.log(
        'Files saves: native menu placement, direct export/import, cancel/invalid preservation, safe restart, exact campaign/profile reload, saved preference and responsive targets passed',
    );
} catch (error) {
    await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
    console.log(
        await page
            .locator('pre')
            .textContent()
            .catch(() => 'No game log'),
    );
    throw error;
} finally {
    await browser.close();
}
