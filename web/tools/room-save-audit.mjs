import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';
import { mkdir } from 'node:fs/promises';
import { pathToFileURL } from 'node:url';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';

async function downloadedBackup(page, button) {
    await expect(button).toBeVisible({ timeout: 60_000 });
    await expect(button).toBeEnabled({ timeout: 60_000 });
    const [download] = await Promise.all([page.waitForEvent('download'), button.click()]);
    const chunks = [];
    for await (const chunk of await download.createReadStream()) chunks.push(chunk);
    return JSON.parse(Buffer.concat(chunks).toString('utf8'));
}

export async function auditRoomSaves(browser, origin, output) {
    const context = await browser.newContext({ viewport: { width: 1440, height: 900 } });
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', (error) => errors.push(String(error)));
    try {
        await page.goto(origin);
        await expect(page.getByRole('region', { name: 'Save transfer' })).toHaveCount(0);
        await page.keyboard.press('F2');
        const importer = page.getByRole('button', { name: 'Restore save backup' });
        const exporter = page.getByRole('button', { name: 'Download save backup' });
        await expect(importer).toBeEnabled({ timeout: 60_000 });
        await expect(exporter).toBeEnabled();
        const canvas = await page.locator('#canvas').elementHandle();
        const original = await downloadedBackup(page, exporter);
        assert.equal(original.format, 'project-midas-saves');
        assert.equal(original.version, 1);
        assert.ok(original.files.every((file) => !/rom|\.z64|\.mstate/i.test(file.path)));
        await expect(page.getByText('Save backup downloaded. It contains no ROM.')).toBeVisible();

        const data = [
            ['Data/Saves/eeprom.bin', Buffer.from(Array.from({ length: 512 }, (_, i) => i % 256))],
            ['Data/Profiles/Portable.geprofile', Buffer.from('private synthetic profile')],
            ['midas.ini', Buffer.from('[display]\nlook_updown=1\nmaxfps=50\n')],
        ];
        const files = data.map(([path, bytes]) => ({
            path,
            data: bytes.toString('base64'),
            sha256: createHash('sha256').update(bytes).digest('hex'),
        }));
        const backup = JSON.stringify({
            format: 'project-midas-saves',
            version: 1,
            createdAt: 'test',
            files,
        });
        const choose = async (text) => {
            await expect(importer).toBeVisible();
            const pending = page.waitForEvent('filechooser');
            await importer.click();
            const picker = await pending;
            assert.equal(await picker.element().getAttribute('accept'), '.json');
            await picker.setFiles({
                name: 'portable-save.json',
                mimeType: 'application/json',
                buffer: Buffer.from(text),
            });
        };
        await choose(backup);
        const confirm = page.getByRole('dialog').filter({ hasText: 'Replace local saves?' });
        await expect(confirm).toBeVisible();
        await confirm.getByRole('button', { name: 'Cancel', exact: true }).click();
        const oldExport = page.getByRole('button', { name: 'Download save backup' });
        assert.deepEqual(
            (await downloadedBackup(page, oldExport)).files,
            original.files,
            'Cancelled import changed saves',
        );
        await choose(JSON.stringify({ ...JSON.parse(backup), version: 99 }));
        await expect(page.getByRole('status').filter({ hasText: 'version 1' })).toBeVisible();
        assert.deepEqual(
            (await downloadedBackup(page, oldExport)).files,
            original.files,
            'Invalid import changed saves',
        );
        await choose(backup);
        await confirm.getByRole('button', { name: 'Restore backup', exact: true }).click();
        await expect(page.getByText('Backup restored. Your saved data is ready.')).toBeVisible();
        const restored = (await downloadedBackup(page, oldExport)).files;
        assert.equal(restored.length, files.length);
        for (const file of files)
            assert.deepEqual(
                restored.find((entry) => entry.path === file.path),
                file,
            );
        assert.ok(
            await canvas.evaluate((element) => element === document.querySelector('#canvas')),
            'Save actions remounted the game canvas',
        );
        await page.goto(origin);
        await page.keyboard.press('F2');
        const reloaded = await downloadedBackup(page, exporter);
        for (const file of files)
            assert.deepEqual(
                reloaded.files.find((entry) => entry.path === file.path),
                file,
                'Save bytes changed after reload',
            );
        await page.locator('.controls-close').click();
        await expect(page.locator('.room-canvas')).toHaveAttribute('data-room-state', 'ready', {
            timeout: 60_000,
        });
        for (const size of [
            { width: 1440, height: 900 },
            { width: 320, height: 568 },
            { width: 390, height: 844 },
            { width: 844, height: 390 },
        ]) {
            await page.setViewportSize(size);
            if (browser.browserType().name() === 'webkit')
                await page.evaluate(() => window.dispatchEvent(new Event('resize')));
            await expect(page.getByRole('region', { name: 'Save transfer' })).toHaveCount(0);
            await page.screenshot({ path: resolve(output, `save-actions-${size.width}.png`) });
        }
        assert.deepEqual(errors, []);
        console.log(
            'Launcher saves: backup/restore, confirmation/cancel, invalid-data protection, exact campaign/profile/settings reload and no room shortcuts passed',
        );
    } finally {
        await context.close();
    }
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
    const origin = process.argv[2] || 'http://127.0.0.1:4176';
    const output = resolve(process.argv[3] || '../build/web-room/save-actions');
    await mkdir(output, { recursive: true });
    const { browser } = await launchAuditBrowser();
    try {
        await auditRoomSaves(browser, origin, output);
    } finally {
        await browser.close();
    }
}
