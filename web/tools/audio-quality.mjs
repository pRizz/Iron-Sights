// Continuity metrics for a quiet, sustained reference scene (not gunshots/music).
export function audioContinuity(samples, rate) {
    let peak = 0,
        energy = 0,
        zero = 0,
        run = 0,
        maxZero = 0,
        gaps = 0,
        jumps = 0,
        maxStep = 0;
    for (let i = 0; i < samples.length; i++) {
        const value = samples[i];
        peak = Math.max(peak, Math.abs(value));
        energy += value * value;
        if (value === 0) {
            zero++;
            run++;
            maxZero = Math.max(maxZero, run);
        } else {
            if (run >= 16) gaps++;
            run = 0;
        }
        if (i) {
            const step = Math.abs(value - samples[i - 1]);
            maxStep = Math.max(maxStep, step);
            if (step > 0.15) jumps++;
        }
    }
    return {
        count: samples.length,
        seconds: samples.length / rate,
        peak,
        rms: samples.length ? Math.sqrt(energy / samples.length) : 0,
        zeroFraction: samples.length ? zero / samples.length : 0,
        maxZero,
        gaps,
        jumps,
        maxStep,
    };
}
