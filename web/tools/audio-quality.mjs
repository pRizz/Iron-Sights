const SCRIPT_PROCESSOR_PERIODS = [256, 512, 1024, 2048, 4096];
const MIN_SILENCE_SAMPLES = 64;

/**
 * Locate output faults in rendered music: exact silence inside audio, an exact
 * repeat of an earlier period (ScriptProcessor replaying a stale buffer), and a
 * hard edge where sound stops or restarts without a fade.
 */
export function audioDropouts(samples, rate, { edge = 0.05 } = {}) {
    const events = [];
    const addEvent = (kind, start, length, detail = {}) =>
        events.push({
            kind,
            atSeconds: start / rate,
            milliseconds: (length * 1000) / rate,
            ...detail,
        });
    let first = 0;
    while (first < samples.length && samples[first] === 0) first++;
    let last = samples.length - 1;
    while (last > first && samples[last] === 0) last--;
    for (let i = first; i <= last;) {
        if (samples[i] !== 0) {
            i++;
            continue;
        }
        let end = i;
        while (end <= last && samples[end] === 0) end++;
        if (end - i >= MIN_SILENCE_SAMPLES) {
            addEvent('silence', i, end - i, {
                edgeIn: Math.abs(samples[i - 1]),
                edgeOut: Math.abs(samples[end]),
                hard: Math.abs(samples[i - 1]) >= edge || Math.abs(samples[end]) >= edge,
            });
        }
        i = end;
    }
    for (const period of SCRIPT_PROCESSOR_PERIODS) {
        for (let i = period; i < samples.length;) {
            let end = i;
            let nonzero = 0;
            while (end < samples.length && samples[end] === samples[end - period]) {
                if (samples[end] !== 0) nonzero++;
                end++;
            }
            if (end - i >= period && nonzero >= period / 2) {
                addEvent('repeat', i, end - i, { period });
            }
            i = Math.max(end, i + 1);
        }
    }
    return events.sort((a, b) => a.atSeconds - b.atSeconds);
}

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
