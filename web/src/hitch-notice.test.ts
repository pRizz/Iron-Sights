import { expect, test } from 'bun:test';
import { createHitchNotice } from './hitch-notice';

test('warns once for a severe gap after stable play', () => {
    let warnings = 0;
    const sample = createHitchNotice(() => warnings++);
    sample(0, true);
    sample(5000, true);
    sample(5016, true);
    sample(6116, true);
    sample(8000, true);
    expect(warnings).toBe(1);
});

test('two recent half-second gaps warn, isolated gaps do not', () => {
    let warnings = 0;
    const sample = createHitchNotice(() => warnings++);
    sample(0, true);
    sample(5000, true);
    sample(5016, true);
    sample(5616, true);
    sample(6016, true);
    expect(warnings).toBe(0);
    sample(6032, true);
    sample(6632, true);
    expect(warnings).toBe(1);
});

test('inactive play resets the baseline and old gap history', () => {
    let warnings = 0;
    const sample = createHitchNotice(() => warnings++);
    sample(0, true);
    sample(5000, true);
    sample(5016, true);
    sample(5600, true);
    sample(6000, false);
    sample(100000, true);
    sample(105000, true);
    sample(105016, true);
    sample(105600, true);
    expect(warnings).toBe(0);
    sample(105616, true);
    sample(106216, true);
    expect(warnings).toBe(1);
});
