import { expect, test } from 'bun:test';
import { installMouseLock } from './mouse-lock';
import { diagnosticRing } from './mouse-diagnostics';

function fixture(request: (options?: PointerLockOptions) => Promise<void> | void) {
    const canvas = { requestPointerLock: request } as unknown as HTMLCanvasElement;
    const errors: unknown[] = [];
    const lock = installMouseLock(canvas, (error) => errors.push(error));
    return { canvas, lock, errors };
}

test('SDL capture requests unadjusted input and restores the original method', async () => {
    const calls: unknown[] = [];
    const original = async (options?: PointerLockOptions) => {
        calls.push(options);
    };
    const { canvas, lock, errors } = fixture(original);
    await canvas.requestPointerLock();
    expect(calls).toEqual([{ unadjustedMovement: true }]);
    expect(lock.mode()).toBe('unadjusted');
    expect(errors).toEqual([]);
    lock.dispose();
    expect(canvas.requestPointerLock).toBe(original);
});

test('unsupported raw input falls back once and remembers compatibility for recapture', async () => {
    const calls: unknown[] = [];
    const { canvas, lock, errors } = fixture(async (options) => {
        calls.push(options);
        if (options?.unadjustedMovement) throw new DOMException('unsupported', 'NotSupportedError');
    });
    await canvas.requestPointerLock();
    await canvas.requestPointerLock();
    expect(calls).toEqual([{ unadjustedMovement: true }, undefined, undefined]);
    expect(lock.pending()).toBe(false);
    expect(lock.mode()).toBe('compatibility');
    expect(errors).toEqual([]);
});

test('denied capture is reported without silently retrying permission failures', async () => {
    let calls = 0;
    const error = new DOMException('denied', 'NotAllowedError');
    const { canvas, lock, errors } = fixture(async () => {
        calls++;
        throw error;
    });
    await expect(canvas.requestPointerLock()).rejects.toBe(error);
    expect(calls).toBe(1);
    expect(errors).toEqual([error]);
    expect(lock.pending()).toBe(false);
});

test('legacy void return and synchronous unsupported raw requests are compatible', async () => {
    const { canvas, lock, errors } = fixture((options) => {
        if (options?.unadjustedMovement) throw new DOMException('unsupported', 'NotSupportedError');
    });
    await canvas.requestPointerLock();
    expect(lock.mode()).toBe('compatibility');
    expect(errors).toEqual([]);
});

test('diagnostic buffers retain only the latest entries in chronological order', () => {
    const ring = diagnosticRing<number>(3);
    ring.add(1);
    ring.add(2);
    expect(ring.read()).toEqual([1, 2]);
    for (const value of [3, 4, 5]) ring.add(value);
    const snapshot = ring.read();
    snapshot.pop();
    expect(ring.read()).toEqual([3, 4, 5]);
});
