import { expect, test } from 'bun:test';
import { containSize, stageFramebuffer } from './presentation';

test('wide viewports contain the framebuffer with side bars', () => {
    expect(containSize(2000, 900, 16 / 9)).toEqual({ width: 1600, height: 900 });
});
test('tall viewports contain the framebuffer with top and bottom bars', () => {
    expect(containSize(800, 1000, 16 / 9)).toEqual({ width: 800, height: 450 });
});
test('presentation follows a changed native framebuffer ratio', () => {
    expect(containSize(1440, 900, 4 / 3)).toEqual({ width: 1200, height: 900 });
});
test('the framebuffer fills a landscape phone stage at its device ratio', () => {
    expect(stageFramebuffer(844, 390, 3)).toEqual({ width: 2532, height: 1170 });
});
test('portrait gets the full width at 4:3, which the renderer draws unstretched', () => {
    expect(stageFramebuffer(390, 844, 3)).toEqual({ width: 1170, height: 878 });
});
test('a 4:3 stage is filled exactly', () => {
    expect(stageFramebuffer(1024, 768, 1)).toEqual({ width: 1024, height: 768 });
});
test('a mouse stage renders at CSS pixels and rounds fractional sizes', () => {
    expect(stageFramebuffer(1279.5, 719.6, 1)).toEqual({ width: 1280, height: 720 });
});
test('an invalid or empty scale falls back to CSS pixels and at least one pixel', () => {
    expect(stageFramebuffer(1024, 768, Number.NaN)).toEqual({ width: 1024, height: 768 });
    expect(stageFramebuffer(0, 0, 2)).toEqual({ width: 1, height: 1 });
});
