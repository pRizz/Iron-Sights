// Private local-ROM diagnostic. Captures/logs stay in the ignored build directory.
import { chromium, firefox, webkit } from '@playwright/test';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { observeAudio, readAudio } from './audio-observer.mjs';
import { observePointer } from './pointer-observer.mjs';

const [romPath, outputPath = '../build/web-audit', origin = 'http://127.0.0.1:4174'] = process.argv.slice(2);
if (!romPath) throw new Error('Usage: runtime-audit.mjs local-rom output-directory [origin]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const choice = process.env.MIDAS_BROWSER || 'chromium';
const engine = choice === 'firefox' ? firefox : choice === 'webkit' ? webkit : chromium;
const options = engine === chromium ? {
  ...(choice === 'chromium' ? {} : { channel: choice }), args: ['--enable-unsafe-swiftshader'],
} : {};
const browser = await engine.launch({ headless: true, ...options });
const context = await browser.newContext({ viewport: { width: 1100, height: 1000 } });
const page = await context.newPage();
if (engine === chromium) {
  const focusSession = await page.context().newCDPSession(page);
  await focusSession.send('Emulation.setFocusEmulationEnabled', { enabled: false });
}
await page.addInitScript(observeAudio);
await page.addInitScript(observePointer);
await page.addInitScript(() => {
  globalThis.midasFocusTrace = [];
  for (const event of ['focus', 'blur', 'visibilitychange']) {
    window.addEventListener(event, () => globalThis.midasFocusTrace.push({
      event, hidden: document.hidden, focus: document.hasFocus(), at: performance.now(),
    }));
  }
});
const messages = [];
page.on('console', message => messages.push(`${message.type()}: ${message.text()}`));
page.on('pageerror', error => messages.push(`PAGE ERROR: ${error.stack}`));
try {
  const auditUrl = new URL(origin);
  auditUrl.searchParams.set('audit', '1');
  await page.goto(auditUrl.href);
  await page.locator('#rom').setInputFiles(resolve(romPath));
  await page.locator('#play').click();
  await page.waitForFunction(() => globalThis.midasPrototype, undefined, { timeout: 90_000 });
  await page.waitForTimeout(6000);
  await page.locator('#resume').click();
  await page.waitForFunction(() => {
    const game = globalThis.midasPrototype;
    const live = game._portWebGetStage() === 33 && game._portWebControlsLive();
    globalThis.midasLiveSince = live ? globalThis.midasLiveSince || performance.now() : 0;
    return live && performance.now() - globalThis.midasLiveSince > 1000;
  }, undefined, { timeout: 90_000 });
  await page.locator('#resume').click();
  await page.waitForTimeout(1000);
  const snapshot = () => page.evaluate(() => {
    const game = globalThis.midasPrototype;
    return {
      frames: game._portWebGetFrames(), tasks: game._portWebGetGfxTasks(),
      position: [0, 1, 2].map(axis => game._portWebGetPlayerPosition(axis)),
      heading: game._portWebGetPlayerHeading(), locked: !!document.pointerLockElement,
      hidden: document.hidden, focus: document.hasFocus(),
    };
  });
  const before = await snapshot();
  await page.keyboard.down('w');
  await page.waitForTimeout(1000);
  await page.keyboard.up('w');
  const moved = await snapshot();
  await page.mouse.move(500, 450);
  await page.mouse.move(650, 450, { steps: 6 });
  await page.waitForTimeout(250);
  const turned = await snapshot();
  const audio = [];
  for (let sample = 0; sample < 4; sample++) {
    audio.push(...await page.evaluate(readAudio));
    await page.waitForTimeout(200);
  }
  await page.screenshot({ path: resolve(output, 'game.png') });
  await writeFile(resolve(output, 'audio.json'), JSON.stringify(audio, null, 2));
  await page.keyboard.down('w');
  const other = await page.context().newPage();
  await other.goto('about:blank');
  await other.bringToFront();
  await other.keyboard.up('w');
  await page.waitForTimeout(250);
  const paused = await snapshot();
  await page.waitForTimeout(350);
  const stillPaused = await snapshot();
  await page.bringToFront();
  await page.locator('#resume').click();
  await page.waitForTimeout(500);
  const resumed = await snapshot();
  await page.waitForTimeout(500);
  const idle = await snapshot();
  await page.keyboard.press('p', { delay: 100 });
  await page.waitForFunction(() => !globalThis.midasPrototype._portWebControlsLive() && !document.pointerLockElement);
  await page.waitForTimeout(3000);
  const watch = await snapshot();
  await page.screenshot({ path: resolve(output, 'watch.png') });
  await page.keyboard.press('p', { delay: 100 });
  await page.waitForFunction(() => globalThis.midasPrototype._portWebControlsLive());
  await page.locator('#resume').click();
  await page.waitForTimeout(250);
  const afterWatch = await snapshot();
  const report = await page.evaluate(() => ({
    browser: navigator.userAgent,
    status: document.querySelector('#status').textContent,
    log: document.querySelector('#log').textContent,
    stage: globalThis.midasPrototype?._portWebGetStage(),
    memory: globalThis.midasPrototype?._portWebGetMemoryBytes(),
    canvas: { width: document.querySelector('canvas').width, height: document.querySelector('canvas').height },
    audioMetrics: Array.from({ length: 10 }, (_, metric) => globalThis.midasPrototype._portWebGetAudioMetric(metric)),
    focusTrace: globalThis.midasFocusTrace,
    lifecycleTrace: globalThis.midasLifecycleTrace,
    pointerTrace: globalThis.midasPointerTrace,
  }));
  const distance = (a, b) => Math.hypot(a.position[0] - b.position[0], a.position[2] - b.position[2]);
  const checks = {
    keyboardDistance: distance(before, moved), mouseTurn: Math.abs(turned.heading - moved.heading),
    pausedFrameDelta: stillPaused.frames - paused.frames, resumedFrameDelta: idle.frames - resumed.frames,
    idleDrift: distance(resumed, idle), audioPeak: Math.max(0, ...audio.map(sample => Math.max(sample.peak, sample.outputPeak))),
    watchAdvancedFrames: watch.frames - idle.frames, watchReleasedMouse: !watch.locked,
    watchResumed: afterWatch.locked,
  };
  await writeFile(resolve(output, 'runtime.json'), JSON.stringify({ ...report, checks, audio,
    before, moved, turned, paused, stillPaused, resumed, idle, watch, afterWatch, messages }, null, 2));
  console.log(JSON.stringify({ browser: report.browser, stage: report.stage, memory: report.memory, checks,
    errors: messages.filter(line => line.includes('ERROR')) }));
  assert.ok(!messages.some(line => line.includes('PAGE ERROR')), 'Browser runtime error');
  assert.ok(!report.pointerTrace.some(event => event.event.endsWith(':rejected')), 'Browser capture request was rejected');
  assert.ok(before.locked && turned.locked, 'Mouse was not captured');
  assert.ok(checks.keyboardDistance > 1, 'Keyboard did not move the player');
  assert.ok(checks.mouseTurn > 0.01, 'Captured mouse did not turn the player');
  assert.equal(checks.pausedFrameDelta, 0, 'Simulation continued while unfocused');
  assert.ok(checks.resumedFrameDelta > 0, 'Simulation failed to resume');
  assert.ok(checks.idleDrift < 5, 'A held movement key remained stuck after focus recovery');
  assert.ok(checks.watchAdvancedFrames > 0 && checks.watchReleasedMouse && checks.watchResumed,
    'Watch menu failed to release/recover input without freezing its animation');
  assert.ok(checks.audioPeak > 0.00001, 'No nonzero samples reached Web Audio');
  assert.ok(audio.some(sample => sample.state === 'running'), 'AudioContext remained suspended');
} catch (error) {
  await page.evaluate(readAudio).then(audio => writeFile(resolve(output, 'failure-audio.json'),
    JSON.stringify({ error: String(error), audio }, null, 2))).catch(() => {});
  await page.evaluate(() => ({
    controls: globalThis.midasPrototype?._portWebControlsLive(),
    audioMetrics: globalThis.midasPrototype ? Array.from({ length: 10 }, (_, metric) => globalThis.midasPrototype._portWebGetAudioMetric(metric)) : [],
    locked: !!document.pointerLockElement, focus: document.hasFocus(), hidden: document.hidden,
    focusTrace: globalThis.midasFocusTrace, log: document.querySelector('#log').textContent,
  })).then(report => writeFile(resolve(output, 'failure-runtime.json'), JSON.stringify(report, null, 2))).catch(() => {});
  await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
  throw error;
} finally {
  await writeFile(resolve(output, 'console.log'), messages.join('\n'));
  await browser.close();
}
