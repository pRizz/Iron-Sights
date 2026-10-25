// QA-only timing probes; no ROM bytes, shader source or screenshots in the report.
export function observeStartup() {
    const trace = { calls: [], frames: [], logs: [], focus: [] };
    globalThis.midasStartupTrace = trace;
    for (const name of [
        'compileShader',
        'linkProgram',
        'getShaderParameter',
        'getProgramParameter',
        'getAttribLocation',
        'getUniformLocation',
        'useProgram',
        'texImage2D',
        'bufferData',
    ]) {
        const original = WebGL2RenderingContext.prototype[name];
        WebGL2RenderingContext.prototype[name] = function (...args) {
            const before = performance.now();
            const result = Reflect.apply(original, this, args);
            const duration = performance.now() - before;
            if (
                this.canvas.id === 'canvas' &&
                (duration > 1 || ['compileShader', 'linkProgram'].includes(name))
            )
                trace.calls.push({ name, at: before, duration });
            return result;
        };
    }
    for (const name of ['focus', 'blur', 'pointerlockchange', 'pointerlockerror']) {
        window.addEventListener(
            name,
            () =>
                trace.focus.push({
                    name,
                    at: performance.now(),
                    focused: document.hasFocus(),
                    locked: !!document.pointerLockElement,
                }),
            true,
        );
    }
    const frame = () => {
        const game = globalThis.midasStartupGame;
        if (game && trace.frames.length < 10000)
            trace.frames.push({
                at: performance.now(),
                gfx: game._portWebGetGfxTasks(),
                live: game._portWebControlsLive(),
                room: !!document.querySelector('.room-layer'),
                resume: !!document.querySelector('.pause-overlay'),
                loading: !!document.querySelector('.startup-screen'),
            });
        requestAnimationFrame(frame);
    };
    requestAnimationFrame(frame);
}

export function readStartup() {
    return globalThis.midasStartupTrace;
}

export async function installStartupProbe(page) {
    await page.addInitScript(observeStartup);
    await page.route('**/runtime/*/ProjectMidas.mjs', async (route) => {
        const response = await route.fetch();
        const source = await response.text();
        await route.fulfill({
            response,
            body: source.replace(
                'export default createMidas;',
                `
export default async function(options) {
    const log = options.print;
    const progress = options.onStartupProgress;
    options.onStartupProgress = (label, completed, total) => {
        const trace = globalThis.midasStartupTrace;
        (trace.progress ||= []).push({at: performance.now(), label, completed, total});
        progress?.(label, completed, total);
    };
    options.print = options.printErr = (line) => {
        globalThis.midasStartupTrace.logs.push({at: performance.now(), line});
        log(line);
    };
    const game = await createMidas(options);
    globalThis.midasStartupGame = game;
    const main = game.callMain;
    game.callMain = (args) => {
        globalThis.midasStartupTrace.main = performance.now();
        return main(args);
    };
    return game;
}`,
            ),
        });
    });
}
