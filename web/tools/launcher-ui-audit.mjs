// ROM-free browser UX regression against the assembled site.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { launchAuditBrowser } from './browser-choice.mjs';

const origin = process.argv[2] || 'http://127.0.0.1:4176';
const { browser } = await launchAuditBrowser();
const context = await browser.newContext({ viewport: { width: 1440, height: 1100 } });
await context.addInitScript(() => {
    if (!navigator.storage) return;
    navigator.storage.persist = async () => false;
    navigator.storage.persisted = async () => false;
});
const page = await context.newPage();
const errors = [];
context.on('page', (target) => target.on('pageerror', (error) => errors.push(String(error))));
page.on('pageerror', (error) => errors.push(String(error)));
async function download(target) {
    const pending = target.waitForEvent('download');
    await target.getByRole('button', { name: 'Download save backup' }).click();
    const file = await pending;
    const stream = await file.createReadStream();
    const chunks = [];
    for await (const chunk of stream) chunks.push(chunk);
    return JSON.parse(Buffer.concat(chunks).toString());
}
const data = Buffer.from('[display]\nmaxfps=50\n');
const restore = JSON.stringify({
    format: 'project-midas-saves',
    version: 1,
    createdAt: 'test',
    files: [
        {
            path: 'midas.ini',
            data: data.toString('base64'),
            sha256: createHash('sha256').update(data).digest('hex'),
        },
    ],
});
try {
    await page.goto(origin);
    await page.getByText('Saved on this device', { exact: true }).waitFor({ timeout: 60_000 });
    const before = await download(page);
    assert.ok(await page.getByRole('button', { name: 'Play GoldenEye' }).isDisabled());
    const transfer = await page.evaluateHandle(() => {
        const value = new DataTransfer();
        value.items.add(new File(['not a ROM'], 'wrong.z64'));
        return value;
    });
    await page.locator('.game-stage').dispatchEvent('drop', { dataTransfer: transfer });
    await page.getByRole('alert').filter({ hasText: 'different size' }).waitFor();
    await page.locator('#backup-file').setInputFiles({
        name: 'wrong.json',
        mimeType: 'application/json',
        buffer: Buffer.from('{"format":"project-midas-saves","version":99,"files":[]}'),
    });
    await page.getByRole('status').filter({ hasText: 'version 1' }).waitFor();
    assert.deepEqual(
        (await download(page)).files,
        before.files,
        'Invalid import changed saved files',
    );
    await page.locator('#backup-file').setInputFiles({
        name: 'valid.json',
        mimeType: 'application/json',
        buffer: Buffer.from(restore),
    });
    await page.getByRole('dialog').waitFor();
    await page.getByRole('button', { name: 'Cancel', exact: true }).click();
    assert.deepEqual((await download(page)).files, before.files, 'Cancel changed saved files');
    await page.locator('#backup-file').setInputFiles({
        name: 'valid.json',
        mimeType: 'application/json',
        buffer: Buffer.from(restore),
    });
    await page
        .getByRole('dialog')
        .getByRole('button', { name: 'Restore backup', exact: true })
        .click();
    await page.getByText('Backup restored. Your saved data is ready.').waitFor();
    assert.deepEqual((await download(page)).files, JSON.parse(restore).files);
    // Abort the actual IDBFS transaction. This injects a quota-style failure;
    // it does not exhaust the host's real storage quota.
    await page.evaluate(() => {
        const put = IDBObjectStore.prototype.put;
        globalThis.restoreIDBPut = () => {
            IDBObjectStore.prototype.put = put;
        };
        IDBObjectStore.prototype.put = function (...args) {
            if (this.name === 'FILE_DATA') {
                this.transaction.abort();
                throw new DOMException('Synthetic storage quota failure', 'QuotaExceededError');
            }
            return put.apply(this, args);
        };
    });
    const failedData = Buffer.from('[display]\nmaxfps=49\n');
    const failedRestore = JSON.stringify({
        ...JSON.parse(restore),
        files: [
            {
                path: 'midas.ini',
                data: failedData.toString('base64'),
                sha256: createHash('sha256').update(failedData).digest('hex'),
            },
        ],
    });
    await page.locator('#backup-file').setInputFiles({
        name: 'quota.json',
        mimeType: 'application/json',
        buffer: Buffer.from(failedRestore),
    });
    await page
        .getByRole('dialog')
        .getByRole('button', { name: 'Restore backup', exact: true })
        .click();
    await page.getByText('Save failed · retry or download a backup', { exact: true }).waitFor();
    assert.deepEqual(
        (await download(page)).files,
        JSON.parse(restore).files,
        'Failed restore changed memory files',
    );
    await page.evaluate(() => globalThis.restoreIDBPut());
    await page.getByRole('button', { name: 'Retry save', exact: true }).click();
    await page.getByText('Saved on this device', { exact: true }).waitFor();
    await page.getByRole('button', { name: 'Request storage protection' }).click();
    await page.getByText('Protection declined · backups remain available').waitFor();
    await page.getByRole('button', { name: 'Fullscreen', exact: true }).click();
    await page.waitForFunction(() => !!document.fullscreenElement);
    await page.getByRole('button', { name: 'Exit fullscreen', exact: true }).click();
    await page.waitForFunction(() => !document.fullscreenElement);
    const second = await context.newPage();
    await second.goto(origin);
    await second.getByRole('alert').filter({ hasText: 'Another Project Midas tab' }).waitFor();
    await page.close();
    await second.waitForFunction(async () => !(await navigator.locks.query()).held.length);
    await second.getByRole('button', { name: 'Try again' }).click();
    await second.getByText('Saved on this device', { exact: true }).waitFor({ timeout: 60_000 });
    assert.deepEqual((await download(second)).files, JSON.parse(restore).files);
    await second.getByRole('checkbox', { name: 'Remember ROM on this device' }).uncheck();
    await second.reload();
    await second.getByText('Saved on this device', { exact: true }).waitFor({ timeout: 60_000 });
    assert.ok(
        !(await second.getByRole('checkbox', { name: 'Remember ROM on this device' }).isChecked()),
    );
    await second.getByRole('checkbox', { name: 'Remember ROM on this device' }).check();
    await second.evaluate(
        () =>
            new Promise((resolve, reject) => {
                const request = indexedDB.open('project-midas-ads:rom', 1);
                request.onerror = () => reject(request.error);
                request.onsuccess = () => {
                    const database = request.result;
                    const transaction = database.transaction('rom', 'readwrite');
                    transaction
                        .objectStore('rom')
                        .put({ blob: new Blob(['bad']), name: 'broken.z64' }, 'original');
                    transaction.oncomplete = () => {
                        database.close();
                        resolve();
                    };
                    transaction.onabort = () => reject(transaction.error);
                };
            }),
    );
    await second.reload();
    await second
        .getByRole('alert')
        .filter({ hasText: 'different size' })
        .waitFor({ timeout: 60_000 });
    assert.ok(
        !(await second.getByText('✓ Remembered for next time').isVisible()),
        'Damaged ROM was shown as usable',
    );
    await second.getByRole('button', { name: 'Forget remembered ROM' }).click();
    await second.getByText('The remembered ROM was removed. Saves are unchanged.').waitFor();
    assert.deepEqual(
        (await download(second)).files,
        JSON.parse(restore).files,
        'Forgetting a bad ROM changed saves',
    );
    assert.deepEqual(errors, []);
    console.log(
        'Launcher UX: invalid drop/import, cancel/restore, quota rollback/retry, download, denied protection, fullscreen, tab lock/retry and preference reload passed',
    );
} catch (error) {
    console.error(
        'Launcher failure state:',
        await page
            .evaluate(() => ({
                alerts: [...document.querySelectorAll('[role="alert"]')].map(
                    (element) => element.textContent,
                ),
                status: document.querySelector('.launch-bar')?.textContent,
            }))
            .catch(() => null),
    );
    throw error;
} finally {
    await browser.close();
}
