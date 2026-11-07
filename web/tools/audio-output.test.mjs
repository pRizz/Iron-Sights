import { afterEach, beforeEach, expect, test } from 'bun:test';
import { readFile } from 'node:fs/promises';

const source = await readFile(new URL('../../port/web/audio_output.js', import.meta.url), 'utf8');
const { midasAudioRing, midasCreateAudioOutput } = new Function(
    'Module',
    `${source}\nreturn { midasAudioRing, midasCreateAudioOutput };`,
)({});

const FADE = 128;
const RESUME = 662;
const SAMPLE = 16384; // exactly 0.5 after scaling

function chunkOf(frames, value = SAMPLE) {
    return new Int16Array(frames * 2).fill(value);
}

function render(ring, frames) {
    const left = new Float32Array(frames);
    const right = new Float32Array(frames);
    ring.render(left, right);
    return { left, right };
}

function ready(ring, frames = 2000) {
    ring.push(chunkOf(frames));
    render(ring, FADE); // past the fade-in
}

test('stays silent until the resume level, then fades in', () => {
    // Arrange
    const ring = midasAudioRing(FADE, RESUME);
    ring.push(chunkOf(RESUME - 1));
    // Act
    const early = render(ring, 128).left;
    ring.push(chunkOf(1000));
    const { left } = render(ring, 256);
    // Assert
    expect(early.every((value) => value === 0)).toBe(true);
    expect(left[0]).toBeCloseTo(0.5 / FADE);
    expect(left[63]).toBeCloseTo((0.5 * 64) / FADE);
    expect(left[200]).toBe(0.5);
});

test('plays at unity while the queue holds more than a fade', () => {
    const ring = midasAudioRing(FADE, RESUME);
    ready(ring);
    const { left, right } = render(ring, 512);
    expect(left.every((value) => value === 0.5)).toBe(true);
    expect(right.every((value) => value === 0.5)).toBe(true);
});

test('an underrun fades smoothly to exact silence and is counted once', () => {
    const ring = midasAudioRing(FADE, RESUME);
    ready(ring, 1000);
    const { left } = render(ring, 1000 - FADE + 300);
    const steps = left.slice(1).map((value, i) => Math.abs(value - left[i]));
    expect(Math.max(...steps)).toBeLessThanOrEqual(0.5 / FADE + 1e-6);
    expect(left.at(-1)).toBe(0);
    expect(ring.stats()).toMatchObject({ level: 0, underruns: 1, playing: false });
});

test('audio arriving during the fade recovers without an underrun', () => {
    const ring = midasAudioRing(FADE, RESUME);
    ready(ring, 1000);
    render(ring, 1000 - FADE - FADE / 2);
    ring.push(chunkOf(2000));
    const { left } = render(ring, 512);
    const steps = left.slice(1).map((value, i) => Math.abs(value - left[i]));
    expect(Math.max(...steps)).toBeLessThanOrEqual(0.5 / FADE + 1e-6);
    expect(left.at(-1)).toBe(0.5);
    expect(ring.stats().underruns).toBe(0);
});

test('reports consumed frames across chunk boundaries and keeps channels apart', () => {
    const ring = midasAudioRing(FADE, RESUME);
    ready(ring, 1000);
    const stereo = new Int16Array(2000);
    for (let i = 0; i < 1000; i++) {
        stereo[i * 2] = 8192;
        stereo[i * 2 + 1] = -8192;
    }
    ring.push(stereo);
    const before = ring.stats().consumed;
    const { left, right } = render(ring, 1000 - FADE + 200); // into the second chunk
    expect(ring.stats().consumed - before).toBe(1000 - FADE + 200);
    expect(left.at(-1)).toBe(0.25);
    expect(right.at(-1)).toBe(-0.25);
});

test('clear plays out one fade, then silence, without counting an underrun', () => {
    const ring = midasAudioRing(FADE, RESUME);
    ready(ring, 3000);
    ring.clear();
    expect(ring.stats().level).toBe(FADE);
    const { left } = render(ring, 300);
    expect(left[0]).toBeCloseTo(0.5, 1);
    expect(left.slice(FADE).every((value) => value === 0)).toBe(true);
    expect(ring.stats()).toMatchObject({ level: 0, consumed: 0, underruns: 0 });
});

test('clear while not playing drops everything queued', () => {
    const ring = midasAudioRing(FADE, RESUME);
    ring.push(chunkOf(100));
    ring.clear();
    expect(ring.stats().level).toBe(0);
    expect(render(ring, 128).left.every((value) => value === 0)).toBe(true);
});

test('audio arriving before the cleared tail ends still counts as consumed', () => {
    // Arrange: resuming from pause refills the queue within the 5.8 ms tail.
    const ring = midasAudioRing(FADE, RESUME);
    ready(ring, 3000);
    ring.clear();
    ring.push(chunkOf(2000));
    // Act
    render(ring, FADE + 500);
    // Assert: only the tail is excluded, or the queue estimate would never drain.
    expect(ring.stats().consumed).toBe(500);
    expect(ring.stats().underruns).toBe(0);
});

class FakePort {
    posted = [];
    postMessage(message) {
        this.posted.push(message);
    }
}

function installFakeAudio({ state = 'running', loads = true } = {}) {
    const created = { nodes: [], contexts: [] };
    globalThis.AudioContext = class {
        sampleRate = 22050;
        currentTime = 0;
        state = state;
        destination = {};
        audioWorklet = {
            addModule: () => (loads ? Promise.resolve() : Promise.reject(new Error('blocked'))),
        };
        constructor({ sampleRate }) {
            this.sampleRate = sampleRate;
            created.contexts.push(this);
        }
        resume() {
            this.state = 'running';
            return Promise.resolve();
        }
        close() {
            this.state = 'closed';
            return Promise.resolve();
        }
    };
    globalThis.AudioWorkletNode = class {
        port = new FakePort();
        connected = true;
        connect() {}
        disconnect() {
            this.connected = false;
        }
        constructor(context) {
            this.context = context;
            created.nodes.push(this);
        }
    };
    created.window = new FakeTarget();
    created.document = Object.assign(new FakeTarget(), { hidden: false });
    globalThis.window = created.window;
    globalThis.document = created.document;
    return created;
}

class FakeTarget {
    listeners = {};
    addEventListener(name, listener) {
        (this.listeners[name] ??= []).push(listener);
    }
    dispatch(name) {
        for (const listener of this.listeners[name] ?? []) listener({ type: name });
    }
}

function hideAndReturn(fake) {
    fake.document.hidden = true;
    fake.document.dispatch('visibilitychange');
    fake.document.hidden = false;
    fake.document.dispatch('visibilitychange');
}

const settle = () => new Promise((resolve) => setTimeout(resolve, 0));

const consoleInfo = console.info;
beforeEach(() => {
    console.info = () => {};
});

afterEach(() => {
    console.info = consoleInfo;
    delete globalThis.AudioContext;
    delete globalThis.AudioWorkletNode;
    delete globalThis.window;
    delete globalThis.document;
});

test('open reports failure without AudioWorklet, so SDL can take over', () => {
    const output = midasCreateAudioOutput({});
    expect(output.open()).toBe(false);
});

test('queue level follows the context clock during a main-thread stall', async () => {
    // Arrange
    const { nodes, contexts } = installFakeAudio();
    const output = midasCreateAudioOutput({});
    expect(output.open()).toBe(true);
    await settle();
    const clear = nodes[0].port.posted.at(-1);
    output.push(new Int16Array(4410 * 2));
    expect(output.queuedBytes()).toBe(4410 * 4);
    // Act: the render thread acknowledges, then the clock moves with no new message.
    nodes[0].port.onmessage({
        data: { generation: clear.generation, consumed: 441, time: 1, playing: true, underruns: 0 },
    });
    contexts[0].currentTime = 1.05; // 1102.5 more frames
    // Assert
    expect(output.queuedBytes()).toBe((4410 - 441 - 1103) * 4);
    contexts[0].currentTime = 5;
    expect(output.queuedBytes()).toBe(0);
});

test('acknowledgements from before a clear are ignored', async () => {
    const { nodes } = installFakeAudio();
    const output = midasCreateAudioOutput({});
    output.open();
    await settle();
    const stale = nodes[0].port.posted.at(-1).generation;
    output.clear();
    output.push(new Int16Array(200));
    nodes[0].port.onmessage({
        data: { generation: stale, consumed: 50, time: 0, playing: false, underruns: 9 },
    });
    expect(output.queuedBytes()).toBe(400);
    expect(output.underruns()).toBe(0);
});

test('a suspended page drains on the wall clock and sends nothing', async () => {
    const { nodes } = installFakeAudio({ state: 'suspended' });
    globalThis.navigator ??= {};
    const output = midasCreateAudioOutput({});
    output.open();
    await settle();
    const before = nodes[0].port.posted.length;
    output.push(new Int16Array(22050 * 2));
    const first = output.queuedBytes();
    await new Promise((resolve) => setTimeout(resolve, 60));
    expect(first).toBeGreaterThan(0);
    expect(output.queuedBytes()).toBeLessThan(first);
    expect(nodes[0].port.posted.length).toBe(before);
});

test('a processor that fails to load reports a full queue, then refuses to reopen', async () => {
    const consoleError = console.error;
    console.error = () => {};
    installFakeAudio({ loads: false });
    const output = midasCreateAudioOutput({});
    output.open();
    await settle();
    console.error = consoleError;
    expect(output.queuedBytes()).toBe(48 * 1024);
    expect(output.open()).toBe(false);
});

test('the context is published where the touch input resumes it', () => {
    installFakeAudio();
    const module = {};
    midasCreateAudioOutput(module).open();
    expect(module.SDL2.audioContext.sampleRate).toBe(22050);
});

test('the first gesture after the page was hidden plays through a new context', async () => {
    // Arrange
    const fake = installFakeAudio();
    const module = {};
    const output = midasCreateAudioOutput(module);
    output.open();
    await settle();
    hideAndReturn(fake);
    // Act
    fake.window.dispatch('touchend');
    await settle();
    output.push(new Int16Array(200));
    // Assert
    const [old, fresh] = fake.contexts;
    expect(old.state).toBe('closed');
    expect(fake.nodes[0].connected).toBe(false);
    expect(module.SDL2.audioContext).toBe(fresh);
    expect(output.node).toBe(fake.nodes[1]);
    expect(fake.nodes[1].port.posted.at(-1).chunk.length).toBe(200);
    expect(output.queuedBytes()).toBe(400);
});

test('a gesture without hiding or interruption keeps the context', async () => {
    const fake = installFakeAudio({ state: 'suspended' });
    const output = midasCreateAudioOutput({});
    output.open();
    await settle();
    fake.window.dispatch('mousedown');
    await settle();
    expect(fake.contexts).toHaveLength(1);
    expect(fake.contexts[0].state).toBe('running');
});

test('an interruption is rebuilt on the next gesture, once', async () => {
    const fake = installFakeAudio();
    const output = midasCreateAudioOutput({});
    output.open();
    await settle();
    fake.contexts[0].state = 'interrupted';
    fake.contexts[0].onstatechange();
    fake.window.dispatch('pointerup');
    fake.window.dispatch('touchend');
    await settle();
    expect(fake.contexts).toHaveLength(2);
    expect(fake.contexts[1].state).toBe('running');
});

test('a processor still loading for the replaced context is not attached', async () => {
    const fake = installFakeAudio();
    const output = midasCreateAudioOutput({});
    output.open();
    hideAndReturn(fake);
    fake.window.dispatch('keydown'); // before the first load settles
    await settle();
    expect(fake.nodes).toHaveLength(1);
    expect(fake.nodes[0].context).toBe(fake.contexts[1]);
    expect(output.node).toBe(fake.nodes[0]);
});
