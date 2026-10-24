// Private local-ROM menu review. Screenshots remain in the ignored build tree.
import { chromium } from '@playwright/test';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';

const [romPath, outputPath = '../build/web-audit/menu', origin = 'http://127.0.0.1:4174'] = process.argv.slice(2);
if (!romPath) throw new Error('Usage: menu-audit.mjs local-rom output-directory [origin]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const maybeChannel = process.env.MIDAS_BROWSER;
const browser = await chromium.launch({ headless: true,
  ...(maybeChannel && maybeChannel !== 'chromium' ? { channel: maybeChannel } : {}), args: ['--enable-unsafe-swiftshader'] });
const page = await browser.newPage({ viewport: { width: 1100, height: 1000 } });
const messages = [];
page.on('pageerror', error => messages.push(error.stack));
try {
  await page.goto(origin);
  await page.locator('#dam').uncheck();
  await page.locator('#rom').setInputFiles(resolve(romPath));
  await page.locator('#play').click();
  await page.waitForFunction(() => globalThis.midasPrototype, undefined, { timeout: 90_000 });
  await page.waitForTimeout(6000);
  await page.locator('#resume').click();
  await page.waitForFunction(() => globalThis.midasPrototype._portWebGetFrames() > 180, undefined, { timeout: 90_000 });
  for (let step = 0; step < 5; step++) {
    await page.keyboard.press('Enter', { delay: 120 });
    await page.waitForTimeout(1800);
    await page.screenshot({ path: resolve(output, `menu-${step}.png`) });
  }
  const report = await page.evaluate(() => ({ frames: midasPrototype._portWebGetFrames(),
    tasks: midasPrototype._portWebGetGfxTasks(), log: document.querySelector('#log').textContent }));
  await writeFile(resolve(output, 'menu.json'), JSON.stringify({ ...report, errors: messages }, null, 2));
  assert.equal(messages.length, 0, 'Menu runtime error');
  assert.ok(report.tasks > 180, 'Menu rendering did not advance');
  console.log(`Menu navigation rendered ${report.tasks} frames without a runtime error; review local screenshots`);
} finally {
  await browser.close();
}
