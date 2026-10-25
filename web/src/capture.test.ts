import { expect, test } from 'bun:test';
import { createCapture } from './capture';
import type { Engine } from './engine';

function captureFixture(touch = false, pointerLock = true) {
    const saved = new Map<string, PropertyDescriptor | undefined>();
    const replace = (name: string, value: unknown) => {
        saved.set(name, Object.getOwnPropertyDescriptor(globalThis, name));
        Object.defineProperty(globalThis, name, { configurable: true, value, writable: true });
    };
    const page = Object.assign(new EventTarget(), {
        hidden: false,
        hasFocus: () => true,
        pointerLockElement: null as EventTarget | null,
        exitPointerLock: () => lock(false),
    });
    const window = new EventTarget();
    const canvas = Object.assign(new EventTarget(), {
        focus: () => {},
        requestPointerLock: () => Promise.resolve(),
    });
    function lock(value: boolean) {
        page.pointerLockElement = value ? canvas : null;
        page.dispatchEvent(new Event('pointerlockchange'));
    }
    const frames = new Map<number, FrameRequestCallback>();
    let next = 0;
    let live = false;
    let deferCapture = false;
    let release = () => {};
    const pauses: number[] = [];
    replace('document', page);
    replace('window', window);
    replace('requestAnimationFrame', (callback: FrameRequestCallback) => {
        frames.set(++next, callback);
        return next;
    });
    replace('cancelAnimationFrame', (id: number) => frames.delete(id));
    const engine = {
        status: () => ({ kind: 'ready' }),
        onPointerRelease: (callback: () => void) => {
            release = callback;
        },
        maybeGame: () => ({
            _portWebSetPaused: (value: number) => pauses.push(value),
            _portWebControlsLive: () => Number(live),
            _portWebGetGfxTasks: () => 100,
            _portWebCaptureMouse: () => {
                if (!deferCapture) lock(true);
                return 0;
            },
        }),
    } as unknown as Engine;
    if (!pointerLock) delete (canvas as Partial<typeof canvas>).requestPointerLock;
    const capture = createCapture(
        engine,
        () => true,
        () => touch,
    );
    const detach = capture.attach(canvas as unknown as HTMLCanvasElement);
    return {
        capture,
        pauses,
        window,
        lock,
        release: () => release(),
        deferCapture: () => {
            deferCapture = true;
        },
        rejectCapture: () => page.dispatchEvent(new Event('pointerlockerror')),
        live: (value: boolean) => {
            live = value;
        },
        step: () => {
            const pending = [...frames.values()];
            frames.clear();
            for (const callback of pending) callback(performance.now());
        },
        dispose: () => {
            detach();
            for (const [name, descriptor] of saved) {
                if (descriptor) Object.defineProperty(globalThis, name, descriptor);
                else Reflect.deleteProperty(globalThis, name);
            }
        },
    };
}

test('cutscene-to-control arrival keeps simulation live while waiting for a capture gesture', () => {
    const fixture = captureFixture();
    try {
        fixture.step();
        fixture.live(true);
        fixture.step();
        fixture.step();
        expect(fixture.capture.state().kind).toBe('mouse-free');
        expect(fixture.pauses).toEqual([]);
    } finally {
        fixture.dispose();
    }
});

test('touch resumes without Pointer Lock and keeps real focus pause', async () => {
    const fixture = captureFixture(true, false);
    try {
        fixture.live(true);
        await fixture.capture.resume();
        fixture.step();
        expect(fixture.capture.state()).toEqual({ kind: 'playing' });
        fixture.window.dispatchEvent(new Event('blur'));
        expect(fixture.capture.state()).toEqual({ kind: 'paused', reason: 'focus' });
        await fixture.capture.resume();
        expect(fixture.capture.state()).toEqual({ kind: 'playing' });
        fixture.live(false);
        fixture.step();
        expect(fixture.capture.state()).toEqual({ kind: 'menu' });
    } finally {
        fixture.dispose();
    }
});

test('an intentional watch release and return do not become focus-loss pauses', () => {
    const fixture = captureFixture();
    try {
        fixture.live(true);
        fixture.lock(true);
        fixture.step();
        fixture.live(false);
        fixture.release();
        fixture.step();
        expect(fixture.capture.state().kind).toBe('menu');
        fixture.live(true);
        fixture.step();
        expect(fixture.capture.state().kind).toBe('mouse-free');
        expect(fixture.pauses).toEqual([]);
    } finally {
        fixture.dispose();
    }
});

test('unexpected mouse release still pauses and an explicit gesture recovers capture', async () => {
    const fixture = captureFixture();
    try {
        fixture.live(true);
        fixture.lock(true);
        fixture.step();
        fixture.lock(false);
        expect(fixture.capture.state()).toEqual({ kind: 'paused', reason: 'mouse' });
        await fixture.capture.resume();
        expect(fixture.capture.state().kind).toBe('playing');
        expect(fixture.pauses).toEqual([1, 0]);
    } finally {
        fixture.dispose();
    }
});

test('real window focus loss still pauses the game', () => {
    const fixture = captureFixture();
    try {
        fixture.live(true);
        fixture.step();
        fixture.window.dispatchEvent(new Event('blur'));
        expect(fixture.capture.state()).toEqual({ kind: 'paused', reason: 'focus' });
        expect(fixture.pauses).toEqual([1]);
    } finally {
        fixture.dispose();
    }
});

test('capture rejection cannot overwrite a real focus pause that arrived while capture was pending', async () => {
    const fixture = captureFixture();
    try {
        fixture.live(true);
        fixture.step();
        fixture.deferCapture();
        const pending = fixture.capture.resume();
        fixture.window.dispatchEvent(new Event('blur'));
        fixture.rejectCapture();
        await pending;
        expect(fixture.capture.state()).toEqual({ kind: 'paused', reason: 'focus' });
    } finally {
        fixture.dispose();
    }
});
