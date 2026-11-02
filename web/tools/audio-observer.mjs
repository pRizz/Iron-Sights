// QA-only Web Audio tap. Never included in the public launcher/runtime bundle.
// Both functions are serialized into the page, so each stays self-contained.

// Tap whichever node feeds the speakers, so this holds for any output backend.
export function observeAudio() {
  const peakOf = (analyser) => {
    const samples = new Float32Array(analyser.fftSize);
    analyser.getFloatTimeDomainData(samples);
    return samples.reduce((largest, value) => Math.max(largest, Math.abs(value)), 0);
  };
  globalThis.midasAudioObservers = [];
  const connect = AudioNode.prototype.connect;
  AudioNode.prototype.connect = function (destination, ...ports) {
    if (!(destination instanceof AudioDestinationNode)) {
      return Reflect.apply(connect, this, [destination, ...ports]);
    }
    const analyser = this.context.createAnalyser();
    analyser.fftSize = 2048;
    Reflect.apply(connect, this, [analyser]);
    const observation = { context: this.context, analyser, outputPeak: 0, blocks: 0 };
    globalThis.midasAudioObservers.push(observation);
    setInterval(() => {
      observation.blocks++;
      observation.outputPeak = Math.max(observation.outputPeak, peakOf(analyser));
    }, 100);
    return Reflect.apply(connect, analyser, [destination, ...ports]);
  };
}

export function readAudio() {
  const peakOf = (analyser) => {
    const samples = new Float32Array(analyser.fftSize);
    analyser.getFloatTimeDomainData(samples);
    return samples.reduce((largest, value) => Math.max(largest, Math.abs(value)), 0);
  };
  return (globalThis.midasAudioObservers || []).map(({ context, analyser, outputPeak, blocks }) => ({
    state: context.state,
    sampleRate: context.sampleRate,
    peak: peakOf(analyser),
    outputPeak,
    blocks,
  }));
}
