import { expect, test } from 'bun:test';
import {
    createTouchFade,
    createTouchingTracker,
    TOUCH_DEFAULT_OPACITY,
    touchRestOpacity,
} from './touch-opacity';

test('the setting percent becomes a CSS opacity', () => {
    expect(touchRestOpacity(50)).toBe(0.5);
    expect(touchRestOpacity(25)).toBe(0.25);
    expect(touchRestOpacity(100)).toBe(1);
    expect(touchRestOpacity(250)).toBe(1);
});

test('before the game reports a value the default strength applies', () => {
    expect(touchRestOpacity(undefined)).toBe(TOUCH_DEFAULT_OPACITY);
    expect(touchRestOpacity(Number.NaN)).toBe(TOUCH_DEFAULT_OPACITY);
    expect(touchRestOpacity(0)).toBe(TOUCH_DEFAULT_OPACITY);
});

function control() {
    return { dataset: {} as Record<string, string> } as unknown as HTMLElement;
}

test('a control stays marked while any finger holds it', () => {
    const tracker = createTouchingTracker();
    const fire = control();
    tracker.press(1, fire);
    tracker.press(2, fire);
    expect(fire.dataset.touching).toBe('true');
    tracker.release({ pointerId: 1 });
    expect(fire.dataset.touching).toBe('true');
    tracker.release({ pointerId: 2 });
    expect(fire.dataset.touching).toBeUndefined();
});

test('a finger moving to a new press releases its old control, and clear forgets all', () => {
    const tracker = createTouchingTracker();
    const stick = control();
    const reload = control();
    tracker.press(1, stick);
    tracker.press(1, reload);
    expect(stick.dataset.touching).toBeUndefined();
    expect(reload.dataset.touching).toBe('true');
    tracker.press(2, stick);
    tracker.clear();
    expect(stick.dataset.touching).toBeUndefined();
    expect(reload.dataset.touching).toBeUndefined();
});

test('touches outside any control and unknown releases are ignored', () => {
    const tracker = createTouchingTracker();
    tracker.press(1, null);
    tracker.release({ pointerId: 9 });
    tracker.clear();
});

test('the fade writes the resting strength to the overlay only when it changes', () => {
    const writes: string[] = [];
    const root = {
        style: { setProperty: (name: string, value: string) => writes.push(`${name}=${value}`) },
    } as unknown as HTMLElement;
    const fade = createTouchFade();
    fade.update(undefined, 50);
    fade.update(root, 50);
    fade.update(root, 50);
    fade.update(root, 75);
    expect(writes).toEqual(['--touch-rest-opacity=0.5', '--touch-rest-opacity=0.75']);
});
