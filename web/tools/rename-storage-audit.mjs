// Private real-ROM acceptance for the URL rename; no migration code ships.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { installGameProbe } from './native-game-audit.mjs';

const [
    rom,
    directory = '../build/iron-sights/rename',
    origin = 'http://127.0.0.1:4179/Iron-Sights/',
] = process.argv.slice(2);
if (!rom)
    throw new Error('Usage: rename-storage-audit.mjs private-rom private-output [new-site-url]');
const output = resolve(directory);
await mkdir(output, { recursive: true });
const current = new URL(origin);
const legacy = new URL('/Project-Midas-ADS/', current); // Deliberate former deployment path.
assert.equal(current.origin, legacy.origin);
const files = [
    ['midas.ini', Buffer.from('[display]\nlook_updown=1\nmaxfps=51\n')],
    ['Data/Saves/eeprom.bin', Buffer.from(Array.from({ length: 512 }, (_, index) => index % 251))],
]
    .map(([path, bytes]) => ({
        path,
        data: bytes.toString('base64'),
        sha256: createHash('sha256').update(bytes).digest('hex'),
    }))
    .sort((left, right) => left.path.localeCompare(right.path));
const backup = { format: 'project-midas-saves', version: 1, createdAt: 'rename-audit', files };
async function exported(page) {
    const pending = page.waitForEvent('download');
    await page.getByRole('button', { name: 'Download save backup' }).click();
    const downloaded = await pending;
    assert.match(downloaded.suggestedFilename(), /^iron-sights-saves-/);
    const chunks = [];
    for await (const chunk of await downloaded.createReadStream()) chunks.push(chunk);
    return JSON.parse(Buffer.concat(chunks).toString()).files;
}
const { browser } = await launchAuditBrowser();
try {
    const page = await browser.newPage({ viewport: { width: 1280, height: 900 } });
    const errors = [];
    page.on('pageerror', (error) => errors.push(String(error)));
    await installGameProbe(page);
    // The same origin serves the legacy route only in this test. Its storage
    // identifiers are the unchanged deployed names, independent of URL path.
    await page.route(legacy.href + '**', async (route) => {
        const url = route.request().url().replace(legacy.href, current.href);
        await route.fulfill({ response: await route.fetch({ url }) });
    });
    await page.goto(legacy.href + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye', exact: true })).toBeEnabled({
        timeout: 90_000,
    });
    await page.locator('#backup-file').setInputFiles({
        name: 'previous-backup.json',
        mimeType: 'application/json',
        buffer: Buffer.from(JSON.stringify(backup)),
    });
    await page
        .getByRole('dialog')
        .getByRole('button', { name: 'Restore backup', exact: true })
        .click();
    await page.getByText('Backup restored. Your saved data is ready.').waitFor();
    assert.deepEqual(await exported(page), files);
    await page.goto(current.href + '?controls');
    await expect(page.getByRole('button', { name: 'Play GoldenEye', exact: true })).toBeEnabled({
        timeout: 90_000,
    });
    assert.deepEqual(await exported(page), files, 'URL rename changed campaign/settings bytes');
    await expect(page).toHaveTitle('Iron Sights · Browser preview');
    await expect(page.locator('.source-link')).toHaveAttribute(
        'href',
        'https://github.com/pRizz/Iron-Sights',
    );
    await page.screenshot({ path: resolve(output, 'remembered-controls.png') });
    await page.goto(current.href);
    await page.waitForFunction(
        () => globalThis.midasAuditGame?._portWebGetGfxTasks() >= 180,
        undefined,
        { timeout: 120_000 },
    );
    await expect(page.locator('.rom-dialog')).toBeHidden();
    await expect(page.locator('.startup-screen')).toHaveCount(0);
    await page.screenshot({ path: resolve(output, 'remembered-start.png') });
    assert.deepEqual(errors, []);
    const result = {
        previous: legacy.pathname,
        current: current.pathname,
        savedFiles: files.map(({ path, sha256 }) => ({ path, sha256 })),
        rememberedRomAutostart: true,
        errors,
    };
    await writeFile(resolve(output, 'rename.json'), JSON.stringify(result, null, 2));
    console.log(
        'URL rename preserves the remembered ROM, campaign/settings bytes, old backup format and autostart.',
    );
} finally {
    await browser.close();
}
