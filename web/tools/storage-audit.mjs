// ROM-free integration checks against real IndexedDB and the Web Locks API.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { chromium } from '@playwright/test';

const bundle = await readFile(new URL('../../build/web-storage-test/storage-driver.js', import.meta.url));
const server = createServer((request, response) => {
  response.setHeader('Content-Type', request.url === '/driver.js' ? 'text/javascript' : 'text/html');
  response.end(request.url === '/driver.js' ? bundle : '<!doctype html><title>Storage regression</title>');
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const origin = `http://127.0.0.1:${server.address().port}`;
const maybeChannel = process.env.MIDAS_BROWSER;
const browser = await chromium.launch({ headless: true, ...(maybeChannel && maybeChannel !== 'chromium' ? { channel: maybeChannel } : {}) });
try {
  const context = await browser.newContext();
  const page = await context.newPage();
  await page.goto(origin);
  const initialize = () => import('/driver.js').then(api => { globalThis.storageTest = api; });
  await page.evaluate(initialize);
  await page.evaluate(async () => {
    globalThis.releaseSaveLock = await storageTest.acquireUserFilesLock();
    const bytes = Uint8Array.from([0x37, 0x80, 0x40, 0x12, 9, 8, 7, 6]);
    await storageTest.rememberRom({ bytes, name: 'synthetic.v64', order: 'v64' });
    bytes.fill(0); // Remembering must have snapshotted the original, not this working buffer.
  });
  const second = await context.newPage();
  await second.goto(origin);
  await second.evaluate(initialize);
  assert.match(await second.evaluate(async () => {
    try { await storageTest.acquireUserFilesLock(); return 'incorrectly acquired'; }
    catch (error) { return error.message; }
  }), /Another Iron Sights tab/);
  await page.evaluate(() => releaseSaveLock());
  await page.waitForFunction(async () => !(await navigator.locks.query()).held.length);
  await second.evaluate(async () => { globalThis.releaseSaveLock = await storageTest.acquireUserFilesLock(); });
  await second.evaluate(() => releaseSaveLock());
  await page.reload();
  await page.evaluate(initialize);
  const original = await page.evaluate(async () => {
    const rom = await storageTest.recalledRom();
    return { name: rom.name, bytes: Array.from(new Uint8Array(await rom.arrayBuffer())) };
  });
  assert.deepEqual(original, { name: 'synthetic.v64', bytes: [0x37, 0x80, 0x40, 0x12, 9, 8, 7, 6] });
  const aborted = await page.evaluate(async () => {
    const put = IDBObjectStore.prototype.put;
    IDBObjectStore.prototype.put = function (...args) {
      const result = Reflect.apply(put, this, args);
      this.transaction.abort();
      return result;
    };
    try {
      await storageTest.rememberRom({ bytes: new Uint8Array(4), name: 'replacement', order: 'z64' });
      return false;
    } catch { return true; }
    finally { IDBObjectStore.prototype.put = put; }
  });
  assert.ok(aborted, 'An aborted storage transaction must reject');
  assert.equal(await page.evaluate(async () => (await storageTest.recalledRom()).name), original.name);
  const fallback = await page.evaluate(async () => {
    const put = IDBObjectStore.prototype.put;
    IDBObjectStore.prototype.put = function (value, ...args) {
      if (value?.blob instanceof Blob) {
        this.transaction.abort();
        throw new DOMException('Synthetic Blob preparation failure', 'DataCloneError');
      }
      return Reflect.apply(put, this, [value, ...args]);
    };
    try {
      const bytes = Uint8Array.from([0x37, 0x80, 0x40, 0x12, 9, 8, 7, 6]);
      const pending = storageTest.rememberRom({ bytes, name: 'byte-fallback.v64', order: 'v64' });
      bytes.fill(0);
      await pending;
      const recalled = await storageTest.recalledRom();
      return { name: recalled.name, bytes: Array.from(new Uint8Array(await recalled.arrayBuffer())) };
    } finally { IDBObjectStore.prototype.put = put; }
  });
  assert.deepEqual(fallback, { name: 'byte-fallback.v64', bytes: original.bytes });
  await page.reload();
  await page.evaluate(initialize);
  assert.deepEqual(await page.evaluate(async () => {
    const rom = await storageTest.recalledRom();
    return Array.from(new Uint8Array(await rom.arrayBuffer()));
  }), original.bytes, 'Byte fallback changed across reload');
  await page.evaluate(() => storageTest.forgetRom());
  assert.equal(await page.evaluate(() => storageTest.recalledRom()), undefined);
  console.log('Browser storage: original-byte reload, Blob-to-byte fallback/reload, aborted-write preservation, delete and concurrent-tab lock passed');
} finally {
  await browser.close();
  await new Promise(resolve => server.close(resolve));
}
