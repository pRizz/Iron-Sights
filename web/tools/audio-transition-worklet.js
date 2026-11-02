// Pass-through recorder on the audio render thread; samples stay in this private audit.
class AudioTransitionRecorder extends AudioWorkletProcessor {
    constructor({ processorOptions }) {
        super();
        this.samples = new Float32Array(Math.ceil(processorOptions.seconds * sampleRate));
        this.written = 0;
        this.startedAt = null;
        this.port.onmessage = () => {
            const samples = this.samples.slice(0, this.written);
            this.port.postMessage({ samples, startedAt: this.startedAt, rate: sampleRate }, [
                samples.buffer,
            ]);
        };
    }

    process(inputs, outputs) {
        const input = inputs[0];
        const output = outputs[0];
        for (let channel = 0; channel < output.length; channel++) {
            if (input[channel]) output[channel].set(input[channel]);
        }
        if (this.startedAt === null) this.startedAt = currentTime;
        const count = Math.min(output[0].length, this.samples.length - this.written);
        if (count > 0) {
            this.samples.set(output[0].subarray(0, count), this.written);
            this.written += count;
        }
        return true;
    }
}

registerProcessor('audio-transition-recorder', AudioTransitionRecorder);
