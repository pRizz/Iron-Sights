import { expect, test } from 'bun:test';
import { createTouchInput, createTouchState, defaultTouch, stickVector } from './touch-input';
import type { Engine } from './engine';

test('touch defaults support phones and pointerless tablets without changing desktop defaults', () => {
    expect(defaultTouch(5, true, false)).toBe(true);
    expect(defaultTouch(5, false, false)).toBe(true);
    expect(defaultTouch(0, false, true)).toBe(false);
    expect(defaultTouch(5, false, true)).toBe(false);
});

test('a touch gesture resumes SDL audio without an unhandled rejection', async () => {
    let resumes = 0;
    const engine = {
        maybeGame: () => ({
            SDL2: {
                audioContext: {
                    state: 'suspended',
                    resume: () => {
                        resumes++;
                        return Promise.reject(new Error('test refusal'));
                    },
                },
            },
        }),
    } as unknown as Engine;
    const touch = createTouchInput(engine, true);
    touch.gesture();
    await new Promise((resolve) => setTimeout(resolve, 0));
    expect(resumes).toBe(1);
});
test('thumbstick has a dead zone, analog strength and circular diagonal clamp', () => {
    expect(stickVector(4, 0, 50)).toEqual({ x: 0, y: 0 });
    const half = stickVector(25, 0, 50);
    expect(half.x).toBeGreaterThan(0.3);
    expect(half.x).toBeLessThan(0.5);
    const diagonal = stickVector(100, -100, 50);
    expect(Math.hypot(diagonal.x, diagonal.y)).toBeCloseTo(1);
    expect(diagonal.y).toBeGreaterThan(0);
    expect(stickVector(NaN, 10, 50)).toEqual({ x: 0, y: 0 });
});
test('simultaneous fingers release independently and reset releases every held action', () => {
    const events: unknown[] = [];
    const state = createTouchState((action, down) => events.push([action, down]));
    state.press(1, 'fire');
    state.press(2, 'aim');
    state.press(3, 'fire');
    state.release(1);
    expect(events).toEqual([
        ['fire', true],
        ['aim', true],
    ]);
    state.release(3);
    expect(events.at(-1)).toEqual(['fire', false]);
    state.clear();
    state.release(2);
    expect(events.at(-1)).toEqual(['aim', false]);
    expect(events.length).toBe(4);
});
