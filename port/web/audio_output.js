/* Browser audio output, linked with --pre-js. SDL's ScriptProcessor runs on the
 * game thread, so a long frame or level load replays its last buffer. This queue
 * lives on the audio render thread and keeps playing while the game is busy. */

// Playback state. Its source text also runs on the render thread, so it stays
// self-contained. Below fadeFrames the output ramps down as the queue runs out,
// so an underrun is a short fade rather than a click.
function midasAudioRing(fadeFrames, resumeFrames) {
    var chunks = [];
    var offset = 0;
    var level = 0;
    var gain = 0;
    var playing = false;
    var tailFrames = 0; // left of a cleared queue; the new accounting excludes them
    var clearedBeforeEmpty = false; // running out after a clear is not an underrun
    var consumed = 0;
    var underruns = 0;
    var step = 1 / fadeFrames;

    function discardAfter(keptFrames) {
        var kept = [];
        var remaining = keptFrames;
        var first = offset;
        for (var i = 0; i < chunks.length && remaining > 0; i++) {
            var frames = (chunks[i].length - first) / 2;
            var used = Math.min(frames, remaining);
            kept.push(chunks[i].subarray(0, first + used * 2));
            remaining -= used;
            first = 0;
        }
        chunks = kept;
        level = keptFrames - remaining;
    }

    return {
        push: function (chunk) {
            chunks.push(chunk);
            level += chunk.length / 2;
        },
        // Pausing plays out one fade of what is queued instead of cutting it off.
        clear: function () {
            discardAfter(playing ? fadeFrames : 0);
            tailFrames = level;
            clearedBeforeEmpty = level > 0;
            consumed = 0;
        },
        render: function (left, right) {
            for (var i = 0; i < left.length; i++) {
                if (!playing && level >= resumeFrames) playing = true;
                if (!playing || level === 0) {
                    if (playing && !clearedBeforeEmpty) underruns++;
                    playing = clearedBeforeEmpty = false;
                    gain = 0;
                    left[i] = right[i] = 0;
                    continue;
                }
                var target = level < fadeFrames ? level / fadeFrames : 1;
                gain = target < gain ? Math.max(target, gain - step) : Math.min(target, gain + step);
                var chunk = chunks[0];
                left[i] = (chunk[offset] / 32768) * gain;
                right[i] = (chunk[offset + 1] / 32768) * gain;
                offset += 2;
                if (offset >= chunk.length) {
                    chunks.shift();
                    offset = 0;
                }
                level--;
                if (tailFrames > 0) tailFrames--;
                else consumed++;
            }
        },
        stats: function () {
            return { level: level, consumed: consumed, underruns: underruns, playing: playing };
        },
    };
}

// Runs in AudioWorkletGlobalScope, built from this function's source text.
function midasAudioProcessorSetup(createRing, options) {
    registerProcessor(
        options.name,
        class extends AudioWorkletProcessor {
            constructor() {
                super();
                this.ring = createRing(options.fadeFrames, options.resumeFrames);
                this.generation = 0;
                this.quanta = 0;
                this.port.onmessage = (event) => {
                    const message = event.data;
                    if (message.clear) {
                        this.generation = message.generation;
                        this.ring.clear();
                    } else if (message.generation === this.generation) {
                        this.ring.push(message.chunk);
                    }
                };
            }

            process(inputs, outputs) {
                const output = outputs[0];
                this.ring.render(output[0], output[1] || output[0]);
                if (++this.quanta >= options.ackQuanta) {
                    this.quanta = 0;
                    const stats = this.ring.stats();
                    stats.generation = this.generation;
                    stats.time = currentTime + output[0].length / sampleRate;
                    this.port.postMessage(stats);
                }
                return true;
            }
        },
    );
}

function midasCreateAudioOutput(module) {
    var RATE = 22050; // the synthesizer's rate; the browser resamples to the device
    var FRAME_BYTES = 4;
    var FAILED_QUEUE_BYTES = 48 * 1024; // never drains: the pump's stall path reopens
    var PROCESSOR = {
        name: 'midas-audio-output',
        fadeFrames: 128, // 5.8 ms
        resumeFrames: 662, // 30 ms
        ackQuanta: 8, // 46 ms
    };
    var context = null;
    var node = null;
    var opened = false;
    var failed = false;
    var resuming = false;
    var stale = false; // hidden or interrupted since this context was made
    var listening = false;
    var generation = 0;
    var sent = 0;
    var ack = { consumed: 0, time: 0, playing: false };
    var underruns = 0;
    var unplayedFrames = 0;
    var unplayedAt = 0;

    function running() {
        return node !== null && context.state === 'running';
    }

    function reset() {
        generation++;
        sent = 0;
        ack = { consumed: 0, time: 0, playing: false };
        unplayedFrames = 0;
        if (node) node.port.postMessage({ clear: true, generation: generation });
    }

    // Until the page may play, frames drain on the wall clock as SDL's silence
    // timer did, so the mixer keeps its pace and the pump sees no stalled device.
    function drainUnplayed() {
        var now = performance.now();
        unplayedFrames = Math.max(0, unplayedFrames - ((now - unplayedAt) * RATE) / 1000);
        unplayedAt = now;
    }

    function resume() {
        var target = context;
        if (!target || target.state === 'running' || target.state === 'closed' || resuming) return;
        resuming = true;
        Promise.resolve(target.resume()).finally(function () {
            if (target === context) resuming = false;
        });
    }

    // iOS can bring a backgrounded or interrupted context back playing a second
    // or more late, and only a new context (or quitting the browser) recovers.
    // The first gesture after one builds a new context, which may start at once.
    function rebuild() {
        var old = context;
        console.info('Audio output rebuilt after the page was hidden or interrupted', {
            state: old.state,
            baseLatency: old.baseLatency,
            outputLatency: old.outputLatency,
            queuedFrames: ack.level,
        });
        old.onstatechange = null;
        if (node) {
            node.port.onmessage = null;
            node.disconnect();
        }
        context = node = null;
        resuming = stale = false;
        Promise.resolve(old.close()).catch(function () {});
        if (!createContext()) failed = true;
        reset();
    }

    function wake() {
        if (stale && context && !document.hidden) rebuild();
        resume();
    }

    function listen() {
        if (listening) return;
        listening = true;
        ['keydown', 'mousedown', 'pointerup', 'touchend'].forEach(function (name) {
            window.addEventListener(name, wake, { capture: true, passive: true });
        });
        document.addEventListener('visibilitychange', function () {
            if (document.hidden) stale = true;
        });
    }

    function loadProcessor() {
        var target = context;
        var source =
            '(' + midasAudioProcessorSetup + ')(' + midasAudioRing + ', ' + JSON.stringify(PROCESSOR) + ');';
        var url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
        context.audioWorklet
            .addModule(url)
            .then(function () {
                if (target !== context) return; // rebuilt while loading
                node = new AudioWorkletNode(context, PROCESSOR.name, {
                    numberOfInputs: 0,
                    outputChannelCount: [2],
                });
                node.port.onmessage = function (event) {
                    if (event.data.generation !== generation) return;
                    ack = event.data;
                    underruns = event.data.underruns;
                };
                node.connect(context.destination);
                reset();
            })
            .catch(function (error) {
                if (target !== context) return;
                failed = true;
                console.error('Audio output processor failed to load', error);
            })
            .finally(function () {
                URL.revokeObjectURL(url);
            });
    }

    function createContext() {
        if (typeof AudioContext === 'undefined' || typeof AudioWorkletNode === 'undefined') return false;
        try {
            context = new AudioContext({ sampleRate: RATE });
        } catch (error) {
            return false;
        }
        if (!context.audioWorklet || context.sampleRate !== RATE) {
            context.close();
            context = null;
            return false;
        }
        // Touch gestures and audits reach the context where SDL publishes its own.
        module['SDL2'] = module['SDL2'] || {};
        module['SDL2'].audioContext = context;
        context.onstatechange = function () {
            if (context.state === 'interrupted') stale = true; // WebKit only
            reset();
        };
        listen();
        loadProcessor();
        return true;
    }

    return {
        get node() {
            return node;
        },
        open: function () {
            if (failed || (!context && !createContext())) return false;
            opened = true;
            reset();
            return true;
        },
        close: function () {
            opened = false;
            reset();
        },
        clear: reset,
        push: function (samples) {
            if (!opened || failed) return;
            var frames = samples.length / 2;
            if (!running()) {
                drainUnplayed();
                unplayedFrames += frames;
                return;
            }
            var chunk = samples.slice();
            sent += frames;
            node.port.postMessage({ chunk: chunk, generation: generation }, [chunk.buffer]);
        },
        // The render thread acknowledges periodically; the context clock, which
        // keeps moving during a stall, covers the time since the last one.
        queuedBytes: function () {
            if (failed) return FAILED_QUEUE_BYTES;
            if (!opened) return 0;
            if (!running()) {
                if (navigator.userActivation && navigator.userActivation.hasBeenActive) resume();
                drainUnplayed();
                return Math.round(unplayedFrames) * FRAME_BYTES;
            }
            var played = ack.playing ? Math.max(0, context.currentTime - ack.time) * RATE : 0;
            return Math.max(0, Math.round(sent - ack.consumed - played)) * FRAME_BYTES;
        },
        underruns: function () {
            return underruns;
        },
    };
}

Module['midasAudioOutput'] = midasCreateAudioOutput(Module);
