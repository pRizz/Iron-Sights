import { expect, test } from 'bun:test';
import { canvasSafeArea } from './safe-area';

const phone = { left: 0, top: 0, width: 844, height: 390 };

test('a landscape iPhone canvas under the notch reports the left inset', () => {
    const area = canvasSafeArea({ left: 47, top: 0, right: 47, bottom: 21 }, phone, phone);
    expect(area.left).toBeCloseTo(47 / 844);
    expect(area.right).toBeCloseTo(47 / 844);
    expect(area.top).toBe(0);
    expect(area.bottom).toBeCloseTo(21 / 390);
});
test('a canvas already clear of the safe area reports nothing', () => {
    const canvas = { left: 60, top: 0, width: 724, height: 360 };
    expect(canvasSafeArea({ left: 47, top: 0, right: 47, bottom: 21 }, canvas, phone)).toEqual({
        left: 0,
        top: 0,
        right: 0,
        bottom: 0,
    });
});
test('a portrait canvas below the status bar reports only what it overlaps', () => {
    const viewport = { left: 0, top: 0, width: 390, height: 844 };
    const canvas = { left: 0, top: 30, width: 390, height: 293 };
    const area = canvasSafeArea({ left: 0, top: 47, right: 0, bottom: 34 }, canvas, viewport);
    expect(area.top).toBeCloseTo(17 / 293);
    expect(area.bottom).toBe(0);
});
test('no insets or an empty canvas report nothing', () => {
    expect(canvasSafeArea({ left: 0, top: 0, right: 0, bottom: 0 }, phone, phone).left).toBe(0);
    const empty = { left: 0, top: 0, width: 0, height: 0 };
    expect(canvasSafeArea({ left: 47, top: 47, right: 0, bottom: 0 }, empty, phone).left).toBe(0);
});
test('an inset never claims more than half the canvas', () => {
    expect(canvasSafeArea({ left: 900, top: 0, right: 0, bottom: 0 }, phone, phone).left).toBe(0.5);
});
