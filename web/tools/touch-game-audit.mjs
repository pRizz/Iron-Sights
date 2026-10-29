// Private real-ROM acceptance using only on-screen controls after file selection.
import assert from 'node:assert/strict';
import { expect } from '@playwright/test';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { launchAuditBrowser } from './browser-choice.mjs';
import { installGameProbe } from './native-game-audit.mjs';
import { observeAudio, readAudio } from './audio-observer.mjs';
import { auditTouchWatch } from './touch-watch-audit.mjs';

const [rom, outputPath = '../build/web-touch/game', origin = 'http://127.0.0.1:4177/Iron-Sights/'] =
    process.argv.slice(2);
if (!rom) throw new Error('Usage: touch-game-audit.mjs private-rom private-output [origin]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const { browser, kind } = await launchAuditBrowser();
const context = await browser.newContext({
    viewport:
        process.env.MIDAS_TABLET === '1'
            ? { width: 1024, height: 768 }
            : { width: 844, height: 390 },
    deviceScaleFactor: Number(process.env.MIDAS_DPR || 2),
    isMobile: true,
    hasTouch: true,
});
await context.addInitScript(() => {
    delete HTMLCanvasElement.prototype.requestPointerLock;
    // Reproduce iOS transferring canvas focus to native on-screen buttons.
    // The page remains focused; this must not become a focus-loss pause.
    document.addEventListener(
        'pointerdown',
        (event) => {
            if (event.target.closest?.('.touch-controls'))
                document.querySelector('#canvas')?.dispatchEvent(new FocusEvent('blur'));
        },
        true,
    );
});
if (kind === 'webkit')
    await context.addInitScript(() => {
        // Desktop WebKit's mobile viewport driver does not expose iOS hardware points.
        Object.defineProperty(Navigator.prototype, 'maxTouchPoints', { get: () => 5 });
    });
const page = await context.newPage();
if (kind === 'chromium') {
    const focus = await context.newCDPSession(page);
    await focus.send('Emulation.setFocusEmulationEnabled', { enabled: false });
}
await installGameProbe(page, { readMemory: !!process.env.MIDAS_TOUCH_WATCH_MAP });
await page.addInitScript(observeAudio);
const errors = [];
page.on('pageerror', (error) => errors.push(String(error)));
const tap = async (locator) => {
    await expect(locator).toBeVisible();
    await locator.scrollIntoViewIfNeeded();
    const box = await locator.boundingBox();
    await page.touchscreen.tap(box.x + box.width / 2, box.y + box.height / 2);
};
const action = (name) => page.locator(`[data-touch-action="${name}"]`);
const sample = () =>
    page.evaluate(() => ({
        position: [0, 1, 2].map((axis) => midasAuditGame._portWebGetPlayerPosition(axis)),
        heading: midasAuditGame._portWebGetPlayerHeading(),
        frames: midasAuditGame._portWebGetFrames(),
    }));
const resize = async (width, height) => {
    await page.setViewportSize({ width, height });
    if (kind === 'webkit') {
        // Desktop WebKit's mobile driver changes innerWidth without dispatching
        // the rotation resize event, and retains the old visualViewport size.
        await page.evaluate(() => window.dispatchEvent(new Event('resize')));
    }
    await page.waitForTimeout(400);
};
try {
    await page.goto(origin + '?controls');
    await expect(page.getByLabel('On-screen touch controls')).toBeChecked();
    await expect(page.getByLabel('Touch layout', { exact: true })).toHaveValue('n64');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({
        timeout: 90000,
    });
    await tap(page.getByRole('button', { name: 'Play GoldenEye' }));
    console.log('Waiting for native frames at full device pixel ratio');
    await page.waitForFunction(
        () => globalThis.midasAuditGame?._portWebGetGfxTasks() >= 240,
        undefined,
        { timeout: 120000 },
    );
    await expect(page.locator('.touch-controls')).toBeVisible();
    await expect(page.locator('.touch-controls')).toHaveAttribute('data-layout', 'n64');
    await page.evaluate(() => {
        globalThis.touchContextAudit = [];
        const overlay = document.querySelector('.touch-controls');
        const record = () =>
            touchContextAudit.push({
                context: overlay.dataset.context,
                buttons: overlay.querySelectorAll('button').length,
            });
        record();
        new MutationObserver(record).observe(overlay, {
            attributes: true,
            attributeFilter: ['data-context'],
        });
    });
    const resolution = await page.evaluate(() => {
        const canvas = document.querySelector('#canvas');
        const viewport = window.visualViewport;
        const width = Math.min(innerWidth, viewport?.width || innerWidth);
        const height = Math.min(innerHeight, viewport?.height || innerHeight);
        const fittedWidth = Math.min(width, (height * 16) / 9);
        return {
            expected: [
                Math.round(fittedWidth * devicePixelRatio),
                Math.round(((fittedWidth * 9) / 16) * devicePixelRatio),
            ],
            actual: [canvas.width, canvas.height],
            css: [canvas.clientWidth, canvas.clientHeight],
            dpr: devicePixelRatio,
            settings: new TextDecoder().decode(midasAuditGame.FS.readFile('/user/midas.ini')),
            drawable: [
                canvas.getContext('webgl2').drawingBufferWidth,
                canvas.getContext('webgl2').drawingBufferHeight,
            ],
        };
    });
    assert.deepEqual(
        resolution.actual,
        resolution.expected,
        'SDL clamped or double-scaled the Retina framebuffer',
    );
    assert.deepEqual(resolution.drawable, resolution.actual);
    console.log('Retina framebuffer', resolution.actual, 'CSS presentation', resolution.css);
    assert.match(resolution.settings, /mouse_auto_aim=1/);
    let filesChecked = false;
    for (let step = 0; step < 24; step++) {
        if (await page.evaluate(() => midasAuditGame._portWebGetStage() === 33)) break;
        await expect(page.locator('.touch-controls button')).toHaveCount(0);
        if (step < 10) await page.screenshot({ path: resolve(output, `navigation-${step}.png`) });
        if (await page.evaluate(() => midasAuditGame._portWebFileMenuMetric(0))) {
            filesChecked = true;
            await expect(page.locator('.touch-n64')).toHaveCount(0);
            await expect(
                page.getByRole('button', { name: 'Import Save', exact: true }),
            ).toBeVisible();
            await page.screenshot({ path: resolve(output, 'files.png') });
            const box = await page.locator('#canvas').boundingBox();
            // The first campaign folder is centered within the 4:3 menu band.
            const menuWidth = Math.min(box.width, (box.height * 4) / 3);
            await page.touchscreen.tap(
                box.x + box.width / 2 - menuWidth * 0.25,
                box.y + box.height * 0.32,
            );
        } else {
            const box = await page.locator('#canvas').boundingBox();
            const menuWidth = Math.min(box.width, (box.height * 4) / 3);
            // Native 448x336 cursor coordinates: first mission tile. The same
            // upper-left point selects Agent and advances the briefing pages.
            await page.touchscreen.tap(
                box.x + box.width / 2 - menuWidth / 2 + (menuWidth * 73) / 448,
                box.y + (box.height * 62) / 336,
            );
        }
        await page.waitForTimeout(1800);
    }
    assert.equal(
        await page.evaluate(() => midasAuditGame._portWebGetStage()),
        33,
        'Cursor-only touch navigation did not reach Dam',
    );
    await page.waitForFunction(() => midasAuditGame._portWebControlsLive(), undefined, {
        timeout: 90000,
    });
    await expect(page.locator('.touch-controls')).toHaveAttribute('data-mode', 'game');
    await expect(page.locator('.touch-controls')).toHaveAttribute('data-context', 'game');
    const contexts = await page.evaluate(() => touchContextAudit);
    assert.ok(
        contexts.some((entry) => entry.context === 'cinematic'),
        'Intro context was not observed',
    );
    assert.ok(
        contexts
            .filter((entry) => ['navigation', 'cinematic'].includes(entry.context))
            .every((entry) => entry.buttons === 0),
        'HUD appeared before live mission input',
    );
    await expect(page.locator('.touch-n64')).toHaveCSS('animation-name', 'touch-hud-enter');
    await expect(page.locator('.touch-n64')).toHaveCSS('opacity', '1');
    await page.screenshot({ path: resolve(output, 'game-ready.png') });
    assert.ok(filesChecked, 'Files-screen touch navigation was not exercised');
    await expect(page.locator('.touch-n64')).toBeVisible();
    assert.equal(await page.evaluate(() => !!document.pointerLockElement), false);
    await expect(page.locator('.pause-overlay')).toHaveCount(0);
    await expect(page.locator('.mouse-capture-hint')).toHaveCount(0);
    const before = await sample();
    await tap(action('aim'));
    await expect(action('aim')).toHaveAttribute('aria-pressed', 'true');
    await page.waitForTimeout(350);
    if (kind === 'chromium') {
        const session = await context.newCDPSession(page);
        const points = new Map();
        const update = async (id, x, y, down) => {
            const existed = points.has(id);
            const ending = points.get(id);
            down ? points.set(id, { id, x, y }) : points.delete(id);
            await session.send('Input.dispatchTouchEvent', {
                type: down ? (existed ? 'touchMove' : 'touchStart') : 'touchEnd',
                touchPoints: down ? [...points.values()] : [ending],
            });
        };
        const stick = await page.locator('.touch-stick').boundingBox();
        const fire = await action('fire').boundingBox();
        const look = await page.locator('.touch-look').boundingBox();
        const sx = stick.x + stick.width / 2,
            sy = stick.y + stick.height / 2;
        await update(1, sx, sy - stick.height * 0.35, true);
        await update(2, look.x + look.width * 0.55, look.y + look.height - 12, true);
        await update(3, fire.x + 24, fire.y + 24, true);
        await page.waitForTimeout(1200);
        await update(2, look.x + look.width * 0.55 + 80, look.y + look.height - 12, true);
        await page.waitForTimeout(250);
        await update(3, 0, 0, false);
        await update(2, 0, 0, false);
        await update(1, 0, 0, false);
    } else {
        // The WebKit driver has no multi-contact touchscreen API; exercise real
        // DOM touch-pointer handlers without claiming native iOS input coverage.
        const box = await page.locator('.touch-stick').boundingBox();
        await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
        await page.mouse.down();
        await page.mouse.move(box.x + box.width / 2, box.y + 12);
        await page.waitForTimeout(500);
        await page.mouse.up();
    }
    const moved = await sample();
    assert.ok(
        Math.hypot(...moved.position.map((value, i) => value - before.position[i])) > 25,
        'Thumbstick did not move Bond',
    );
    if (kind === 'chromium') {
        assert.ok(Math.abs(moved.heading - before.heading) > 2, 'Touch drag did not turn Bond');
    }
    await tap(action('aim'));
    await tap(action('watch'));
    await expect(page.locator('.touch-controls')).toHaveAttribute('data-mode', 'menu', {
        timeout: 10000,
    });
    await expect(page.locator('.touch-controls')).toHaveAttribute('data-context', 'watch');
    await expect(page.locator('.pause-overlay')).toHaveCount(0);
    await auditTouchWatch(page, context);
    await tap(action('down'));
    await tap(action('watch'));
    await expect(page.locator('.touch-controls')).toHaveAttribute('data-mode', 'game', {
        timeout: 10000,
    });
    await expect(page.locator('.touch-game-hint')).toBeVisible();
    await page.waitForTimeout(5500);
    await expect(page.locator('.touch-game-hint')).toHaveCount(0);
    await expect(page.locator('.touch-look span')).toHaveCount(0);
    await expect(
        page.locator('.touch-controls').getByRole('button', { name: /^(Pause|Options)$/ }),
    ).toHaveCount(0);
    // The overlay deliberately has no browser Pause/Options buttons. Window
    // focus loss must still suspend simulation and offer the normal Resume.
    await page.evaluate(() => window.dispatchEvent(new Event('blur')));
    await expect(page.locator('.pause-overlay')).toBeVisible();
    await expect(page.locator('.touch-controls')).toHaveCount(0);
    const paused = await sample();
    await page.waitForTimeout(700);
    assert.equal((await sample()).frames, paused.frames, 'Touch pause did not stop simulation');
    await tap(page.getByRole('button', { name: 'Resume game' }));
    await expect(page.locator('.touch-controls')).toBeVisible();
    await expect(page.locator('.touch-controls')).toBeVisible();
    const performanceStart = await page.evaluate(() => ({
        time: performance.now(),
        frames: midasAuditGame._portWebGetGfxTasks(),
    }));
    await page.waitForTimeout(1200);
    const performanceEnd = await page.evaluate(() => ({
        time: performance.now(),
        frames: midasAuditGame._portWebGetGfxTasks(),
    }));
    const renderedFps =
        ((performanceEnd.frames - performanceStart.frames) * 1000) /
        (performanceEnd.time - performanceStart.time);
    const audio = await page.evaluate(readAudio);
    const audioAvailable = await page.evaluate(
        () => !!(window.AudioContext || window.webkitAudioContext),
    );
    if (audioAvailable)
        assert.ok(
            audio.some((s) => s.state === 'running' && Math.max(s.peak, s.outputPeak) > 0.00001),
            'Touch gestures did not unlock game audio',
        );
    await resize(390, 844);
    for (const element of [page.locator('.touch-stick'), action('fire'), action('watch')]) {
        const box = await element.boundingBox();
        assert.ok(
            box.x >= 0 && box.y >= 0 && box.x + box.width <= 391 && box.y + box.height <= 845,
            'Portrait control cropped',
        );
    }
    await page.screenshot({ path: resolve(output, 'portrait.png') });
    await resize(1024, 768);
    assert.deepEqual(
        await page.locator('#canvas').evaluate((canvas) => [canvas.width, canvas.height]),
        resolution.actual,
        'Rotation changed the chosen framebuffer resolution',
    );
    await page.screenshot({ path: resolve(output, 'tablet.png') });
    assert.deepEqual(errors, []);
    await writeFile(
        resolve(output, 'report.json'),
        JSON.stringify(
            {
                before,
                resolution,
                renderedFps,
                moved,
                audio,
                audioAvailable,
                errors,
                contexts,
                checks: 'cursor-only startup/menus to Dam, hidden HUD during intro, native input-ready fade, Z firing, thumbstick, Start watch, hidden Pause/Options, focus pause/resume, audio, portrait/tablet bounds',
                driver:
                    kind === 'webkit'
                        ? 'desktop WebKit: simulated touch capability/resize; single pointer drag'
                        : 'Chromium: simultaneous touch contacts',
            },
            null,
            2,
        ),
    );
    console.log(
        'Touch game: cursor-only menus, hidden intro HUD, engine-ready fade, native actions, watch, audio and portrait/tablet bounds passed',
    );
} catch (error) {
    await writeFile(
        resolve(output, 'layout.json'),
        JSON.stringify(
            await page
                .evaluate(() => ({
                    inner: [innerWidth, innerHeight],
                    visual: [visualViewport?.width, visualViewport?.height],
                    stage: document.querySelector('.game-stage')?.getBoundingClientRect().toJSON(),
                    controls: [...document.querySelectorAll('.touch-controls button')].map((e) => ({
                        name: e.textContent,
                        box: e.getBoundingClientRect().toJSON(),
                    })),
                }))
                .catch(() => ({})),
        ),
    );
    await writeFile(
        resolve(output, 'native-log.txt'),
        await page
            .locator('pre')
            .textContent()
            .catch(() => ''),
    );
    await writeFile(
        resolve(output, 'audio.json'),
        JSON.stringify(
            await page
                .evaluate(() => ({
                    samples: globalThis.midasAudioObservers,
                    state: globalThis.midasAuditGame?.SDL2?.audioContext?.state,
                    metrics: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map((i) =>
                        globalThis.midasAuditGame?._portWebGetAudioMetric(i),
                    ),
                }))
                .catch(() => ({})),
        ),
    );
    await page.screenshot({ path: resolve(output, 'failure.png') }).catch(() => {});
    await writeFile(resolve(output, 'failure.txt'), String(error));
    throw error;
} finally {
    await browser.close();
}
