import { expect, test } from 'bun:test';
import { menuTapMoved } from './touch-input';

test('menu taps tolerate finger wobble up to ten CSS pixels', () => {
    expect(menuTapMoved(106, 108, 100, 100)).toBe(false);
    expect(menuTapMoved(100, 100, 100, 100)).toBe(false);
});

test('menu panning exceeds the tap slop in either direction', () => {
    expect(menuTapMoved(111, 100, 100, 100)).toBe(true);
    expect(menuTapMoved(92, 92, 100, 100)).toBe(true);
});
