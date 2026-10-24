// QA-only Web Audio tap. Never included in the public launcher/runtime bundle.
export function observeAudio() {
  const maybeContext = window.AudioContext || window.webkitAudioContext;
  if (!maybeContext) return;
  const original = maybeContext.prototype.createScriptProcessor;
  globalThis.midasAudioObservers = [];
  maybeContext.prototype.createScriptProcessor = function (...args) {
    const node = Reflect.apply(original, this, args);
    const analyser = this.createAnalyser();
    analyser.fftSize = 2048;
    const connect = node.connect.bind(node);
    node.connect = (destination, ...ports) => {
      if (destination !== this.destination) return connect(destination, ...ports);
      connect(analyser);
      return analyser.connect(destination, ...ports);
    };
    const observation = { context: this, analyser, outputPeak: 0, blocks: 0 };
    node.addEventListener('audioprocess', event => queueMicrotask(() => {
      observation.blocks++;
      for (let channel = 0; channel < event.outputBuffer.numberOfChannels; channel++) {
        for (const value of event.outputBuffer.getChannelData(channel)) {
          observation.outputPeak = Math.max(observation.outputPeak, Math.abs(value));
        }
      }
    }));
    globalThis.midasAudioObservers.push(observation);
    return node;
  };
}

export function readAudio() {
  return (globalThis.midasAudioObservers || []).map(({ context, analyser, outputPeak, blocks }) => {
    const samples = new Float32Array(analyser.fftSize);
    analyser.getFloatTimeDomainData(samples);
    const peak = samples.reduce((largest, value) => Math.max(largest, Math.abs(value)), 0);
    return { state: context.state, sampleRate: context.sampleRate, peak, outputPeak, blocks };
  });
}
