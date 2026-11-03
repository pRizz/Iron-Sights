// Private real-ROM audit for single-frame stray geometry in the browser game.
// The default run fires repeatedly in Dam and fails on a large one-frame flash.
// MIDAS_FLASH_MANUAL=1 instead lets a player press F8 to keep the last few seconds.
// Saved frames are ROM-derived and stay in the ignored output directory.
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';

const [
    rom,
    folder = '../build/web-render-flash/current',
    origin = 'http://127.0.0.1:4177/Iron-Sights/',
] = process.argv.slice(2);
if (!rom) throw new Error('Usage: render-flash-audit.mjs private-rom private-output [origin]');
const output = resolve(folder);
await mkdir(output, { recursive: true });
const manual = process.env.MIDAS_FLASH_MANUAL === '1';
const gameSeconds = Number(process.env.MIDAS_FLASH_GAME_SECONDS || (manual ? 1200 : 150));
const manualFrames = manual ? Number(process.env.MIDAS_FLASH_RING_FRAMES || 180) : 0;
const traceDraws = process.env.MIDAS_FLASH_TRACE_DRAWS === '1';
const NEWLINE = String.fromCharCode(10);

// Where the original report was captured: a pistol held at a fence in Dam.
const FIRING_SPAWN = '12922,58,1944,123.6';
// A flash this large only came from casings stuck at the camera. Muzzle flashes
// reached 70 cells on the web and 98 on native in the same scenario.
const FLASH_FAILURE_CELLS = 100;

// GE_INPUTPLAY records: Z taps with pauses that let the gun reload.
function firingInput() {
    const records = ['P 0 0 0 0'];
    let tick = 200;
    let shots = 0;
    while (tick < 14000) {
        records.push(`P ${tick} 2000 0 0`, `P ${tick + 4} 0 0 0`);
        tick += 18;
        shots++;
        if (shots % 7 === 0) tick += 120;
    }
    return records.join(NEWLINE) + NEWLINE;
}

// Runs in the page. A flash differs from both neighbours while they agree.
function observeFlashes({ maxSaved, minSavedCells, manualFrames, traceDraws }) {
    const GRID_X = 96;
    const GRID_Y = 54;
    const CHANGED = 18; // mean channel difference for a grid cell to count as changed
    const SETTLED = 10; // the neighbours must agree this closely around the flash
    const MIN_CELLS = 6;
    const state = {
        events: [],
        incidents: [],
        frames: 0,
        saved: 0,
        lastSavedTasks: -100,
        started: false,
    };
    globalThis.midasFlash = state;

    // Record draws that reach the camera plane, or fill the screen, as the GPU gets them.
    let drawBatch = [];
    if (traceDraws) {
        const proto = WebGL2RenderingContext.prototype;
        const { bufferData, drawArrays, useProgram, vertexAttribPointer } = proto;
        let vertices = null;
        let program = null;
        const position = { stride: 0, offset: 0 };
        proto.useProgram = function (next) {
            program = next;
            return Reflect.apply(useProgram, this, [next]);
        };
        proto.bufferData = function (target, data, usage, sourceOffset, length) {
            if (target === this.ARRAY_BUFFER && ArrayBuffer.isView(data) && length) {
                const start = data.byteOffset + sourceOffset * data.BYTES_PER_ELEMENT;
                vertices = new Float32Array(
                    data.buffer.slice(start, start + length * data.BYTES_PER_ELEMENT),
                );
            }
            return Reflect.apply(bufferData, this, [target, data, usage, sourceOffset, length]);
        };
        proto.vertexAttribPointer = function (index, size, type, normalized, stride, offset) {
            if (program && index === this.getAttribLocation(program, 'aVtxPos')) {
                position.stride = stride / 4;
                position.offset = offset / 4;
            }
            return Reflect.apply(vertexAttribPointer, this, [
                index,
                size,
                type,
                normalized,
                stride,
                offset,
            ]);
        };
        proto.drawArrays = function (mode, first, count) {
            const { stride, offset } = position;
            if (
                this.canvas.id === 'canvas' &&
                mode === this.TRIANGLES &&
                vertices &&
                stride &&
                count <= 36
            ) {
                const ws = [];
                for (let v = first; v < first + count; v++)
                    ws.push(vertices[v * stride + offset + 3]);
                if (Math.min(...ws) < 5 && drawBatch.length < 80) {
                    drawBatch.push({
                        count,
                        stride,
                        blend: this.isEnabled(this.BLEND),
                        depthTest: this.isEnabled(this.DEPTH_TEST),
                        depthMask: this.getParameter(this.DEPTH_WRITEMASK),
                        floats: Array.from(
                            vertices.subarray(first * stride, (first + count) * stride),
                        ),
                    });
                }
            }
            return Reflect.apply(drawArrays, this, [mode, first, count]);
        };
    }

    const getContext = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (kind, attributes) {
        const wanted =
            this.id === 'canvas' ? { ...attributes, preserveDrawingBuffer: true } : attributes;
        return Reflect.apply(getContext, this, [kind, wanted]);
    };

    const signature = (pixels, width, height) => {
        const cells = new Float32Array(GRID_X * GRID_Y * 3);
        const counts = new Uint32Array(GRID_X * GRID_Y);
        for (let y = 0; y < height; y += 2) {
            const row = Math.floor((y * GRID_Y) / height);
            for (let x = 0; x < width; x += 2) {
                const cell = row * GRID_X + Math.floor((x * GRID_X) / width);
                const at = (y * width + x) * 4;
                cells[cell * 3] += pixels[at];
                cells[cell * 3 + 1] += pixels[at + 1];
                cells[cell * 3 + 2] += pixels[at + 2];
                counts[cell]++;
            }
        }
        for (let cell = 0; cell < counts.length; cell++) {
            for (let channel = 0; channel < 3; channel++)
                cells[cell * 3 + channel] /= counts[cell] || 1;
        }
        return cells;
    };
    const cellDifference = (a, b, cell) =>
        (Math.abs(a[cell * 3] - b[cell * 3]) +
            Math.abs(a[cell * 3 + 1] - b[cell * 3 + 1]) +
            Math.abs(a[cell * 3 + 2] - b[cell * 3 + 2])) /
        3;
    const toPng = (pixels, width, height) => {
        const canvas = document.createElement('canvas');
        canvas.width = width;
        canvas.height = height;
        const context = canvas.getContext('2d');
        const image = context.createImageData(width, height);
        for (let y = 0; y < height; y++) {
            const from = (height - 1 - y) * width * 4;
            image.data.set(pixels.subarray(from, from + width * 4), y * width * 4);
        }
        context.putImageData(image, 0, 0);
        return canvas.toDataURL('image/png');
    };
    state.pngOf = (frame) => toPng(frame.pixels, frame.width, frame.height);

    const ring = [];
    if (manualFrames) {
        window.addEventListener(
            'keydown',
            (event) => {
                if (event.key !== 'F8' || event.repeat) return;
                event.preventDefault();
                state.incidents.push(ring.slice());
            },
            true,
        );
    }

    const detect = (before, flash, after) => {
        const hits = [];
        for (let cell = 0; cell < GRID_X * GRID_Y; cell++) {
            if (
                cellDifference(flash.cells, before.cells, cell) >= CHANGED &&
                cellDifference(flash.cells, after.cells, cell) >= CHANGED &&
                cellDifference(before.cells, after.cells, cell) <= SETTLED
            )
                hits.push(cell);
        }
        return hits;
    };

    let previousGfxTasks = -1;
    const history = [];
    function sample() {
        requestAnimationFrame(sample);
        const game = globalThis.midasAuditGame;
        if (!state.started || !game) return;
        const tasks = game._portWebGetGfxTasks();
        if (tasks === previousGfxTasks) return;
        previousGfxTasks = tasks;
        const gl = document.querySelector('#canvas').getContext('webgl2');
        const { drawingBufferWidth: width, drawingBufferHeight: height } = gl;
        const pixels = new Uint8Array(width * height * 4);
        gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
        state.frames++;
        const details = {
            at: performance.now(),
            tasks,
            stage: game._portWebGetStage(),
            live: game._portWebControlsLive(),
            position: [0, 1, 2].map((axis) => game._portWebGetPlayerPosition(axis)),
            heading: game._portWebGetPlayerHeading(),
        };
        if (manualFrames) {
            ring.push({ ...details, pixels, width, height });
            if (ring.length > manualFrames) ring.shift();
            return;
        }
        const draws = drawBatch;
        drawBatch = [];
        history.push({
            ...details,
            draws,
            pixels,
            width,
            height,
            cells: signature(pixels, width, height),
        });
        if (history.length > 3) history.shift();
        if (history.length < 3) return;
        const [before, flash, after] = history;
        const hits = detect(before, flash, after);
        if (hits.length < MIN_CELLS) return;
        const xs = hits.map((cell) => cell % GRID_X);
        const ys = hits.map((cell) => Math.floor(cell / GRID_X));
        const event = {
            at: flash.at,
            tasks: flash.tasks,
            cells: hits.length,
            box: [
                Math.min(...xs),
                GRID_Y - 1 - Math.max(...ys),
                Math.max(...xs),
                GRID_Y - 1 - Math.min(...ys),
            ],
            live: flash.live,
            position: flash.position,
            heading: flash.heading,
        };
        const distinct = flash.tasks - state.lastSavedTasks > 15;
        if (state.saved < maxSaved && flash.live && hits.length >= minSavedCells && distinct) {
            state.saved++;
            state.lastSavedTasks = flash.tasks;
            event.draws = flash.draws;
            event.png = {
                before: toPng(before.pixels, width, height),
                flash: toPng(flash.pixels, width, height),
                after: toPng(after.pixels, width, height),
            };
        }
        state.events.push(event);
    }
    requestAnimationFrame(sample);
}

// Save the oldest waiting incident frame by frame, so one call cannot exhaust Node.
async function saveIncidents(page, savedSoFar) {
    let saved = savedSoFar;
    while (await page.evaluate(() => globalThis.midasFlash.incidents.length)) {
        const count = await page.evaluate(() => globalThis.midasFlash.incidents[0].length);
        const incident = resolve(output, `incident-${String(++saved).padStart(2, '0')}`);
        await mkdir(incident, { recursive: true });
        const frames = [];
        for (let index = 0; index < count; index++) {
            const { url, ...frame } = await page.evaluate((at) => {
                const { pixels, ...details } = globalThis.midasFlash.incidents[0][at];
                return {
                    ...details,
                    url: globalThis.midasFlash.pngOf(globalThis.midasFlash.incidents[0][at]),
                };
            }, index);
            frames.push(frame);
            await writeFile(
                resolve(incident, `frame-${String(index).padStart(3, '0')}.png`),
                Buffer.from(url.split(',')[1], 'base64'),
            );
        }
        await writeFile(resolve(incident, 'frames.json'), JSON.stringify(frames, null, 2));
        await page.evaluate(() => globalThis.midasFlash.incidents.shift());
        console.log(`Saved incident ${saved}: ${count} frames in ${incident}`);
    }
    return saved;
}

async function playManualSession(page) {
    await page.evaluate(() => {
        globalThis.midasFlash.started = true;
    });
    console.log(
        `Hindsight capture ready for ${gameSeconds} seconds: play, and press F8 after a glitch`,
    );
    // Lets a headless run exercise the save path without a person at the keyboard.
    const maybeTestKeySeconds = Number(process.env.MIDAS_FLASH_TEST_KEY_SECONDS);
    if (maybeTestKeySeconds)
        setTimeout(() => void page.keyboard.press('F8'), maybeTestKeySeconds * 1000);
    const until = Date.now() + gameSeconds * 1000;
    let saved = 0;
    // Closing the window ends a manual session; what was saved stays on disk.
    try {
        while (Date.now() < until && !page.isClosed()) {
            saved = await saveIncidents(page, saved);
            await page.waitForTimeout(500);
        }
    } catch (error) {
        if (!page.isClosed()) throw error;
    }
    console.log(`Hindsight capture finished with ${saved} incidents`);
}

async function runFiringScenario(page) {
    await page.waitForFunction(
        () =>
            globalThis.midasAuditGame._portWebGetStage() === 33 &&
            globalThis.midasAuditGame._portWebControlsLive(),
        undefined,
        { timeout: 120000 },
    );
    await page.evaluate(() => {
        globalThis.midasFlash.started = true;
    });
    await page.waitForTimeout(gameSeconds * 1000);
    const result = await page.evaluate(() => {
        const { events, frames } = globalThis.midasFlash;
        return { events, frames };
    });
    const events = [];
    for (const [index, event] of result.events.entries()) {
        const { png, ...summary } = event;
        events.push(summary);
        if (!png) continue;
        for (const [name, url] of Object.entries(png))
            await writeFile(
                resolve(output, `flash-${String(index).padStart(3, '0')}-${name}.png`),
                Buffer.from(url.split(',')[1], 'base64'),
            );
    }
    const live = events.filter((event) => event.live);
    const largest = Math.max(0, ...live.map((event) => event.cells));
    await writeFile(
        resolve(output, 'report.json'),
        JSON.stringify({ frames: result.frames, largest, events }, null, 2),
    );
    console.log(JSON.stringify({ frames: result.frames, flashes: live.length, largest }));
    assert.ok(result.frames > 1000, 'Too few frames were checked to judge this run');
    assert.ok(
        largest < FLASH_FAILURE_CELLS,
        `A ${largest}-cell flash appeared; inspect saved frames`,
    );
}

const { browser } = await launchAuditBrowser();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
const firing = {
    GE_STAGE: 'dam',
    GE_DIFF: '0',
    GE_STAGE_AFTER: '120',
    GE_CHEATS: 'invuln',
    GE_SEED: '1',
    GE_NOINPUT: '1',
    GE_SKIPINTRO: '1',
    GE_SPAWNXYZ: FIRING_SPAWN,
    GE_INPUTPLAY: '/route-input.txt',
};
await page.route('**/runtime/*/ProjectMidas.mjs', async (route) => {
    const response = await route.fetch();
    const source = await response.text();
    await route.fulfill({
        response,
        body: source.replace(
            'export default createMidas;',
            `export default async function(options) {
    const previous = options.preRun;
    options.preRun = (module) => {
        previous?.(module);
        if (!${manual}) {
            module.FS.writeFile('/route-input.txt', ${JSON.stringify(firingInput())});
            Object.assign(module.ENV, ${JSON.stringify(firing)});
        }
    };
    globalThis.midasAuditGame = await createMidas(options);
    return globalThis.midasAuditGame;
}`,
        ),
    });
});
await page.addInitScript(observeFlashes, {
    maxSaved: Number(process.env.MIDAS_FLASH_MAX_SAVED || 6),
    minSavedCells: Number(process.env.MIDAS_FLASH_MIN_SAVED || FLASH_FAILURE_CELLS),
    manualFrames,
    traceDraws,
});
const logs = [];
page.on('console', (message) => {
    if (logs.length < 8192) logs.push(`${message.type()}: ${message.text()}`);
});
page.on('pageerror', (error) => logs.push(`pageerror: ${error}`));

let failure;
try {
    await page.goto(origin + '?controls');
    await page.locator('#rom-file').setInputFiles(resolve(rom));
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({
        timeout: 90000,
    });
    await page.getByRole('button', { name: 'Play GoldenEye' }).click();
    await page.waitForFunction(
        () => globalThis.midasAuditGame?._portWebGetGfxTasks() >= 240,
        undefined,
        { timeout: 120000 },
    );
    if (await page.locator('.controls-close').count())
        await page.locator('.controls-close').click();
    if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
        await page.getByRole('button', { name: 'Resume game' }).click();
    if (manual) await playManualSession(page);
    else await runFiringScenario(page);
} catch (error) {
    failure = error;
    await writeFile(resolve(output, 'failure.txt'), String(error));
} finally {
    await writeFile(resolve(output, 'console.txt'), logs.join(NEWLINE));
    await browser.close();
}
if (failure) throw failure;
