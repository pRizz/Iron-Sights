// Private local-ROM diagnostic. Captures/logs stay in the ignored build directory.
import { chromium } from '@playwright/test';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';

const [romPath, outputPath = '../build/web-audit', origin = 'http://127.0.0.1:4174'] = process.argv.slice(2);
if (!romPath) throw new Error('Usage: runtime-audit.mjs local-rom output-directory [origin]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const browser = await chromium.launch({ channel: process.env.MIDAS_BROWSER || 'chromium', headless: true, args: ['--enable-unsafe-swiftshader'] });
const page = await browser.newPage({ viewport: { width: 1100, height: 1000 } });
const messages = [];
page.on('console', message => messages.push(`${message.type()}: ${message.text()}`));
page.on('pageerror', error => messages.push(`PAGE ERROR: ${error.stack}`));
try {
  await page.goto(origin);
  await page.locator('#rom').setInputFiles(resolve(romPath));
  await page.locator('#play').click();
  await page.waitForFunction(() => globalThis.midasPrototype, undefined, { timeout: 90_000 });
  await page.waitForTimeout(6000);
  await page.locator('#resume').click();
  await page.waitForTimeout(15_000);
  await page.screenshot({ path: resolve(output, 'chromium-dam.png') });
  const report = await page.evaluate(() => ({
    browser: navigator.userAgent,
    status: document.querySelector('#status').textContent,
    log: document.querySelector('#log').textContent,
    stage: globalThis.midasPrototype?._portWebGetStage(),
    memory: globalThis.midasPrototype?._portWebGetMemoryBytes(),
  }));
  await writeFile(resolve(output, 'chromium.json'), JSON.stringify({ ...report, messages }, null, 2));
  console.log(JSON.stringify({ browser: report.browser, status: report.status, stage: report.stage, memory: report.memory, errors: messages.filter(line => line.includes('ERROR')) }));
} finally {
  await writeFile(resolve(output, 'console.log'), messages.join('\n'));
  await browser.close();
}
