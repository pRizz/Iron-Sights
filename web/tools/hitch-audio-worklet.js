// Pass-through probe on the audio render thread; only aggregate timing leaves it.
class HitchAudioProbe extends AudioWorkletProcessor {
    constructor() {
        super();
        this.previous = new Float32Array(128);
        this.blocks = 0;
        this.repeats = 0;
        this.nonzero = 0;
        this.peak = 0;
        this.energy = 0;
        this.samples = 0;
        this.nextReport = currentTime + 0.25;
    }

    process(inputs, outputs) {
        const input = inputs[0];
        const output = outputs[0];
        for (let channel = 0; channel < output.length; channel++) {
            if (input[channel]) output[channel].set(input[channel]);
        }
        const mono = output[0];
        let equal = true;
        let blockPeak = 0;
        for (let i = 0; i < mono.length; i++) {
            const value = mono[i];
            const absolute = Math.abs(value);
            blockPeak = Math.max(blockPeak, absolute);
            this.peak = Math.max(this.peak, absolute);
            this.energy += value * value;
            if (value !== this.previous[i]) equal = false;
            this.previous[i] = value;
        }
        this.blocks++;
        this.samples += mono.length;
        if (blockPeak > 0.001) {
            this.nonzero++;
            if (equal) this.repeats++;
        }
        if (currentTime >= this.nextReport) {
            this.port.postMessage({
                contextTime: currentTime, blocks: this.blocks,
                nonzero: this.nonzero, repeatedNonzero: this.repeats,
                peak: this.peak, rms: Math.sqrt(this.energy / this.samples),
            });
            this.blocks = this.nonzero = this.repeats = this.samples = 0;
            this.peak = this.energy = 0;
            this.nextReport = currentTime + 0.25;
        }
        return true;
    }
}

registerProcessor('hitch-audio-probe', HitchAudioProbe);
