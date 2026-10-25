import { expect, test } from 'bun:test';
import { containSize } from './presentation';

test('wide viewports contain the framebuffer with side bars', () => {
    expect(containSize(2000, 900, 16 / 9)).toEqual({ width: 1600, height: 900 });
});
test('tall viewports contain the framebuffer with top and bottom bars', () => {
    expect(containSize(800, 1000, 16 / 9)).toEqual({ width: 800, height: 450 });
});
test('presentation follows a changed native framebuffer ratio', () => {
    expect(containSize(1440, 900, 4 / 3)).toEqual({ width: 1200, height: 900 });
});
