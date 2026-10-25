import { expect, test } from 'bun:test';
import { advanceQuality } from './quality';

test('isolated compilation hitches and normal idle pacing retain quality', () => {
    const initial = { scale: 1, slowFrames: 0 };
    expect(advanceQuality(advanceQuality(initial, 2000), 34)).toEqual(initial);
});
test('sustained slow rendering lowers resolution in bounded steps', () => {
    let state = { scale: 1, slowFrames: 0 };
    for (let index = 0; index < 8; index++) state = advanceQuality(state, 150);
    expect(state.scale).toBe(0.75);
    for (let index = 0; index < 24; index++) state = advanceQuality(state, 150);
    expect(state.scale).toBe(0.5);
    expect(advanceQuality(state, 150)).toEqual(state);
});
test('frequent slow frames still lower quality when a few frames are fast', () => {
    let state = { scale: 1, slowFrames: 0 };
    for (let index = 0; index < 8; index++) {
        state = advanceQuality(state, 200);
        state = advanceQuality(state, 200);
        state = advanceQuality(state, 34);
    }
    expect(state.scale).toBe(0.75);
});
