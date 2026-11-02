import { expect, test } from 'bun:test';
import { audioContinuity, audioDropouts } from './audio-quality.mjs';

const toneAt = (i) => Math.sin((i * Math.PI * 2 * 441) / 48000) * 0.3 + Math.sin(i * 0.0173) * 0.2;

test('dropouts ignore leading and trailing silence around continuous audio', () => {
    const tone = Array.from({ length: 4800 }, (_, i) => toneAt(i));
    const samples = [...Array(1000).fill(0), ...tone, ...Array(1000).fill(0)];
    expect(audioDropouts(samples, 48000)).toEqual([]);
});

test('a hard silent gap inside audio is reported with its time and length', () => {
    const samples = Array.from({ length: 9600 }, (_, i) => toneAt(i));
    samples.fill(0, 4800, 7200);
    samples[4799] = 0.3;
    const [event, ...others] = audioDropouts(samples, 48000);
    expect(others).toEqual([]);
    expect(event.kind).toBe('silence');
    expect(event.atSeconds).toBeCloseTo(0.1);
    expect(event.milliseconds).toBeCloseTo(50);
    expect(event.hard).toBe(true);
});

test('a faded gap is reported as soft', () => {
    const samples = Array.from({ length: 9600 }, (_, i) => toneAt(i) * 0.01);
    samples.fill(0, 4800, 7200);
    const [event] = audioDropouts(samples, 48000);
    expect(event.hard).toBe(false);
});

test('a replayed stale buffer is reported as a repeat at its period', () => {
    const samples = Array.from({ length: 9600 }, (_, i) => toneAt(i));
    for (let i = 4800; i < 7000; i++) samples[i] = samples[i - 512];
    const events = audioDropouts(samples, 48000).filter((event) => event.kind === 'repeat');
    expect(events.some((event) => event.period === 512)).toBe(true);
});

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
