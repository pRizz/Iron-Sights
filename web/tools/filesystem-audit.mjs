// Real classic FS/IDBFS with synthetic bytes and C stdio; no game ROM required.
import { readFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import assert from 'node:assert/strict';
import { chromium } from '@playwright/test';

const assets = new Map(await Promise.all([
  ['/driver.js', '../../build/web-storage-test/storage-driver.js', 'text/javascript'],
  ['/web_fs_probe.mjs', '../../build/web/files/web_fs_probe.mjs', 'text/javascript'],
  ['/web_fs_probe.wasm', '../../build/web/files/web_fs_probe.wasm', 'application/wasm'],
].map(async ([path, file, mime]) => [path, { body: await readFile(new URL(file, import.meta.url)), mime }])));
const server = createServer((request, response) => {
  const asset = assets.get(request.url);
  response.setHeader('Content-Type', asset?.mime || 'text/html');
  response.end(asset?.body || '<!doctype html><title>Filesystem regression</title>');
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const origin = `http://127.0.0.1:${server.address().port}`;
const maybeChannel = process.env.MIDAS_BROWSER;
let maybeBrowser;
try {
  maybeBrowser = await chromium.launch({ headless: true,
    ...(maybeChannel && maybeChannel !== 'chromium' ? { channel: maybeChannel } : {}) });
  const page = await maybeBrowser.newPage();
  const initialize = async (failRestore = false) => {
    const api = await import('/driver.js');
    const { default: create } = await import('/web_fs_probe.mjs');
    globalThis.fixture = await create({ noInitialRun: true });
    globalThis.saveStatuses = [];
    if (failRestore) {
      const sync = fixture.FS.syncfs;
      fixture.FS.syncfs = (populate, done) => populate ? sync(populate, done) : done(new Error('Synthetic restore commit failure'));
    }
    globalThis.files = await api.mountPersistentFiles(fixture.FS, fixture.IDBFS, status => saveStatuses.push(status));
    globalThis.backups = api;
    fixture.FS.mkdirTree('/user/Data/Saves');
    fixture.FS.mkdirTree('/user/Data/Profiles');
  };
  await page.goto(origin);
  await page.evaluate(initialize);
  await page.evaluate(async () => {
    if (fixture._testWriteCampaign(17)) throw new Error('C campaign write failed');
    fixture.FS.writeFile('/user/Data/Profiles/Test.geprofile', 'synthetic profile');
    await files.flush();
  });
  assert.equal(await page.evaluate(() => saveStatuses.at(-1).kind), 'saved');
  await page.reload();
  await page.evaluate(initialize);
  assert.deepEqual(await page.evaluate(() => Array.from(files.snapshot().find(file => file.path.endsWith('eeprom.bin')).bytes)),
    Array.from({ length: 512 }, (_, index) => (17 + index) & 255));
  // Exercise successive real C closes inside the same millisecond/transaction window.
  await page.evaluate(async () => {
    fixture._testWriteCampaign(18);
    const first = files.flush();
    fixture._testWriteCampaign(19);
    await first;
    await files.flush();
  });
  await page.reload();
  await page.evaluate(initialize);
  assert.equal(await page.evaluate(() => fixture.FS.readFile('/user/Data/Saves/eeprom.bin')[0]), 19);
  assert.ok(await page.evaluate(async () => {
    const before = await backups.createBackup(files.snapshot(), 'test');
    const write = fixture.FS.writeFile;
    let injected = false;
    fixture.FS.writeFile = (path, bytes) => {
      write(path, bytes);
      if (!injected && path.endsWith('/New.geprofile')) { injected = true; throw new Error('Synthetic memory write failure'); }
    };
    let rejected = false;
    try { await files.replace([{ path: 'Data/Profiles/New.geprofile', bytes: new Uint8Array([1, 2, 3]) }]); }
    catch { rejected = true; }
    finally { fixture.FS.writeFile = write; }
    return rejected && before === await backups.createBackup(files.snapshot(), 'test');
  }), 'A failed filesystem write must roll back partial replacement files');
  const rollback = await page.evaluate(async () => {
    const before = await backups.createBackup(files.snapshot(), 'test');
    const sync = fixture.FS.syncfs;
    fixture.FS.syncfs = (_populate, finished) => finished(new DOMException('Synthetic quota failure', 'QuotaExceededError'));
    let rejected = false;
    try { await files.replace([{ path: 'midas.ini', bytes: new TextEncoder().encode('[display]\nmaxfps=30\n') }]); }
    catch { rejected = true; }
    finally { fixture.FS.syncfs = sync; }
    const after = await backups.createBackup(files.snapshot(), 'test');
    return { rejected, unchanged: before === after, pending: files.pending(), status: saveStatuses.at(-1).kind };
  });
  assert.deepEqual(rollback, { rejected: true, unchanged: true, pending: true, status: 'error' });
  // Reload before retry: the prior durable campaign/profile must also be intact.
  await page.reload();
  await page.evaluate(initialize);
  assert.equal(await page.evaluate(() => fixture.FS.readFile('/user/Data/Saves/eeprom.bin')[0]), 19);
  await page.evaluate(async () => {
    const text = await backups.createBackup(files.snapshot(), 'test');
    const restored = await backups.parseBackup(text);
    await files.replace([]);
    await files.replace(restored);
    files.markStarted();
  });
  assert.match(await page.evaluate(async () => {
    try { await files.replace([]); return 'incorrectly restored'; }
    catch (error) { return error.message; }
  }), /Reload the page/);
  await page.reload();
  await page.evaluate(initialize);
  assert.equal(await page.evaluate(() => fixture.FS.readFile('/user/Data/Saves/eeprom.bin')[0]), 19);
  assert.equal(await page.evaluate(() => new TextDecoder().decode(fixture.FS.readFile('/user/Data/Profiles/Test.geprofile'))),
    'synthetic profile');
  // A confirmed in-game import stages a backup without touching the old game's
  // files; a fresh runtime applies it before it can cache campaign data.
  assert.ok(await page.evaluate(async () => {
    const sync = fixture.FS.syncfs;
    fixture.FS.syncfs = (_populate, finished) => finished(new DOMException('Synthetic staging quota failure', 'QuotaExceededError'));
    let rejected = false;
    try { await files.stageRestore([{ path: 'midas.ini', bytes: new TextEncoder().encode('[display]\nalways_health=0\n') }]); }
    catch { rejected = true; }
    finally { fixture.FS.syncfs = sync; }
    await files.flush();
    return rejected && !fixture.FS.analyzePath('/user/.pending-save-restore.json').exists
      && fixture.FS.readFile('/user/Data/Saves/eeprom.bin')[0] === 19;
  }), 'A failed staging sync must leave the live campaign intact without a pending import');
  await page.evaluate(async () => {
    files.markStarted();
    await files.stageRestore([
      { path: 'Data/Saves/eeprom.bin', bytes: new Uint8Array(512).fill(37) },
      { path: 'Data/Profiles/Imported.geprofile', bytes: new TextEncoder().encode('imported profile') },
      { path: 'midas.ini', bytes: new TextEncoder().encode('[display]\nalways_health=1\n') },
    ]);
  });
  assert.equal(await page.evaluate(() => fixture.FS.readFile('/user/Data/Saves/eeprom.bin')[0]), 19);
  assert.ok(await page.evaluate(() => files.snapshot().every(file => !file.path.includes('pending'))));
  await page.reload();
  await assert.rejects(page.evaluate(initialize, true), /Synthetic restore commit failure/);
  assert.equal(await page.evaluate(() => fixture.FS.readFile('/user/Data/Saves/eeprom.bin')[0]), 19);
  assert.equal(await page.evaluate(() => fixture.FS.analyzePath('/user/.pending-save-restore.json').exists), true);
  await page.reload();
  await page.evaluate(initialize);
  assert.equal(await page.evaluate(() => fixture.FS.readFile('/user/Data/Saves/eeprom.bin')[0]), 37);
  assert.equal(await page.evaluate(() => new TextDecoder().decode(fixture.FS.readFile('/user/Data/Profiles/Imported.geprofile'))), 'imported profile');
  assert.equal(await page.evaluate(() => fixture.FS.analyzePath('/user/Data/Profiles/Test.geprofile').exists), false);
  assert.equal(await page.evaluate(() => fixture.FS.analyzePath('/user/.pending-save-restore.json').exists), false);
  await page.reload();
  await page.evaluate(initialize);
  assert.equal(await page.evaluate(() => fixture.FS.readFile('/user/Data/Saves/eeprom.bin')[0]), 37);
  console.log('IDBFS: C save/reload, writes during sync, backup round trip, rollback, running-game guard and staged import/reload passed');
} finally {
  await maybeBrowser?.close();
  await new Promise(resolve => server.close(resolve));
}
