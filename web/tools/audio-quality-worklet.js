// Private diagnostic: capture the rendered stream on the audio thread, after
// ScriptProcessor deadlines. Inspecting its callback alone misses late blocks.
class AudioQualityProbe extends AudioWorkletProcessor {
    constructor(options) {
        super();
        this.samples = new Float32Array(sampleRate * options.processorOptions.seconds);
        this.offset = 0;
    }
    process(inputs, outputs) {
        const input = inputs[0];
        const output = outputs[0];
        for (let channel = 0; channel < output.length; channel++) {
            if (input[channel]) output[channel].set(input[channel]);
        }
        if (this.offset < this.samples.length) {
            const count = Math.min(output[0].length, this.samples.length - this.offset);
            this.samples.set(output[0].subarray(0, count), this.offset);
            this.offset += count;
            if (this.offset === this.samples.length)
                this.port.postMessage(this.samples, [this.samples.buffer]);
        }
        return true;
    }
}
registerProcessor('audio-quality-probe', AudioQualityProbe);
