import { expect, test } from 'bun:test';
import { audioContinuity } from './audio-quality.mjs';

test('continuous reference tone has no silent gaps or sudden sample jumps', () => {
    const tone = Array.from(
        { length: 48000 },
        (_, i) => Math.sin((i * Math.PI * 2 * 440) / 48000) * 0.1,
    );
    const result = audioContinuity(tone, 48000);
    expect(result.seconds).toBe(1);
    expect(result.gaps).toBe(0);
    expect(result.jumps).toBe(0);
    expect(result.rms).toBeCloseTo(Math.SQRT1_2 * 0.1);
});

test('render-thread silence inside nonzero callback output exposes a missed deadline', () => {
    const samples = [...Array(128).fill(0.2), ...Array(256).fill(0), ...Array(128).fill(0.2)];
    const result = audioContinuity(samples, 48000);
    expect(result.maxZero).toBe(256);
    expect(result.gaps).toBe(1);
    expect(result.jumps).toBe(2);
    expect(result.zeroFraction).toBe(0.5);
});

test('buffer-boundary jumps can occur without a silent gap', () => {
    const result = audioContinuity([...Array(128).fill(0.1), ...Array(128).fill(-0.1)], 48000);
    expect(result.maxZero).toBe(0);
    expect(result.jumps).toBe(1);
    expect(result.maxStep).toBeCloseTo(0.2);
});
