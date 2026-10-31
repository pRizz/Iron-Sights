// Private browser timing probe. Reports timing and counters, never game data.
export function observeHitches({ graphics = true, invulnerable = false } = {}) {
    const trace = {
        frames: [], longTasks: [], samples: [], graphics: [], syncs: [],
        audio: [], logs: [], markers: [], focus: [],
    };
    globalThis.midasHitchTrace = trace;
    const add = (name, value, limit = 4096) => {
        if (trace[name].length < limit) trace[name].push(value);
    };

    globalThis.midasHitchBeforeCreate = (options) => {
        if (invulnerable) {
            const previousPreRun = options.preRun;
            options.preRun = (module) => {
                previousPreRun?.(module);
                module.ENV.GE_CHEATS = 'invuln';
                module.ENV.GE_SEED = '1';
            };
        }
        for (const name of ['print', 'printErr']) {
            const original = options[name];
            options[name] = (line) => {
                add('logs', { at: performance.now(), line: String(line) });
                original?.(line);
            };
        }
    };
    globalThis.midasHitchAttach = (game) => {
        globalThis.midasAuditGame = game;
        const sync = game.FS.syncfs.bind(game.FS);
        game.FS.syncfs = (populate, callback) => {
            const at = performance.now();
            return sync(populate, (error) => {
                add('syncs', { at, duration: performance.now() - at, populate, error: !!error });
                callback(error);
            });
        };
        return game;
    };

    let previousFrame = 0;
    function frame(now) {
        const gap = previousFrame ? now - previousFrame : 0;
        if (gap >= 35)
            add('frames', { at: now, gap, stage: globalThis.midasAuditGame?._portWebGetStage() });
        previousFrame = now;
        requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);

    if (PerformanceObserver.supportedEntryTypes?.includes('longtask')) {
        new PerformanceObserver((list) => {
            for (const entry of list.getEntries())
                add('longTasks', { at: entry.startTime, duration: entry.duration });
        }).observe({ type: 'longtask', buffered: true });
    }

    for (const name of graphics ? ['compileShader', 'linkProgram', 'texImage2D', 'texSubImage2D', 'bufferData'] : []) {
        const original = WebGL2RenderingContext.prototype[name];
        WebGL2RenderingContext.prototype[name] = function (...args) {
            const at = performance.now();
            const result = Reflect.apply(original, this, args);
            const duration = performance.now() - at;
            if (this.canvas.id === 'canvas' && duration >= 4)
                add('graphics', { at, name, duration });
            return result;
        };
    }

    for (const name of ['focus', 'blur', 'visibilitychange', 'pointerlockchange'])
        window.addEventListener(name, () => add('focus', {
            at: performance.now(), name, hidden: document.hidden,
            focused: document.hasFocus(), locked: !!document.pointerLockElement,
        }), true);

    setInterval(() => {
        const game = globalThis.midasAuditGame;
        if (!game) return;
        add('samples', {
            at: performance.now(), stage: game._portWebGetStage(),
            pumps: game._portWebGetFrames(), graphics: game._portWebGetGfxTasks(),
            memory: game._portWebGetMemoryBytes(),
            audioQueue: game._portWebGetAudioMetric(1),
            audioFrames: game._portWebGetAudioMetric(2),
            audioPumps: game._portWebGetAudioMetric(3),
        }, 2000);
    }, 250);
}

export async function installHitchAudio(source) {
    const game = globalThis.midasAuditGame;
    const context = game.SDL2?.audioContext;
    const node = game.SDL2?.audio?.scriptProcessorNode;
    if (!context || !node || !context.audioWorklet) return 'unavailable';
    const url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
    try {
        await context.audioWorklet.addModule(url);
    } finally {
        URL.revokeObjectURL(url);
    }
    const probe = new AudioWorkletNode(context, 'hitch-audio-probe', { outputChannelCount: [2] });
    probe.port.onmessage = (event) => {
        if (globalThis.midasHitchTrace.audio.length < 2000)
            globalThis.midasHitchTrace.audio.push(event.data);
    };
    node.disconnect();
    node.connect(probe);
    probe.connect(context.destination);
    globalThis.midasHitchAudioClock = () => context.getOutputTimestamp();
    return 'running';
}
