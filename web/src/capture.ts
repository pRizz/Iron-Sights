import { createSignal } from 'solid-js';
import type { Engine } from './engine';

export type Playback =
    | { kind: 'stopped' | 'starting' | 'menu' | 'playing' | 'capturing' }
    | { kind: 'paused'; reason: 'focus' | 'mouse' | 'ready' | 'manual' };

export function createCapture(engine: Engine, started: () => boolean) {
    const [state, setState] = createSignal<Playback>({ kind: 'stopped' });
    const [notice, setNotice] = createSignal('');
    let maybeCanvas: HTMLCanvasElement | undefined;
    let intentional = false;
    let wasLocked = false;
    let liveSince = 0;
    let pending = false;

    function releaseFromGame(): void {
        if (document.pointerLockElement !== maybeCanvas) return;
        intentional = true;
        document.exitPointerLock();
    }
    engine.onPointerRelease(releaseFromGame);

    function pause(reason: 'focus' | 'mouse' | 'ready' | 'manual'): void {
        if (!started() || engine.status().kind === 'error') return;
        releaseFromGame();
        engine.maybeGame()?._portWebSetPaused(1);
        setState({ kind: 'paused', reason });
    }

    async function resume(): Promise<void> {
        const game = engine.maybeGame();
        const canvas = maybeCanvas;
        if (!game || !canvas || !started() || pending) return;
        pending = true;
        setNotice('');
        canvas.focus();
        try {
            game._portWebSetPaused(0);
            if (!game._portWebControlsLive()) {
                setState({ kind: 'menu' });
                return;
            }
            setState({ kind: 'capturing' });
            await new Promise<void>((resolve, reject) => {
                const done = (maybeError?: Error) => {
                    clearTimeout(timer);
                    document.removeEventListener('pointerlockchange', changed);
                    document.removeEventListener('pointerlockerror', failed);
                    maybeError ? reject(maybeError) : resolve();
                };
                const changed = () => {
                    if (document.pointerLockElement === canvas) done();
                };
                const failed = () =>
                    done(new Error('Mouse capture was refused. Click Resume to try again.'));
                const timer = setTimeout(failed, 5000);
                document.addEventListener('pointerlockchange', changed);
                document.addEventListener('pointerlockerror', failed);
                if (game._portWebCaptureMouse() < 0) failed();
            });
            if (state().kind === 'paused' || document.hidden || !document.hasFocus()) {
                pause('focus');
                return;
            }
            setState({ kind: 'playing' });
        } catch (error) {
            setNotice(
                error instanceof Error ? error.message : 'Mouse capture failed. Try Resume again.',
            );
            pause('mouse');
        } finally {
            pending = false;
        }
    }

    function attach(canvas: HTMLCanvasElement): () => void {
        maybeCanvas = canvas;
        const events = new AbortController();
        const listen = { signal: events.signal };
        // SDL's pinned SDK ignores the returned Promise; handle rejection on this
        // canvas while retaining pointerlockerror for the capture state machine.
        const request = canvas.requestPointerLock.bind(canvas);
        canvas.requestPointerLock = (options) => {
            const result = request(options);
            void result?.catch(() => {});
            return result;
        };
        document.addEventListener(
            'pointerlockchange',
            () => {
                const locked = document.pointerLockElement === canvas;
                const lost = wasLocked && !locked;
                wasLocked = locked;
                if (!locked && intentional) {
                    intentional = false;
                    return;
                }
                if (lost && !pending) pause('mouse');
            },
            listen,
        );
        window.addEventListener('blur', () => pause('focus'), listen);
        document.addEventListener(
            'visibilitychange',
            () => {
                if (document.hidden) pause('focus');
            },
            listen,
        );
        canvas.addEventListener(
            'blur',
            () => {
                if (!pending) pause('focus');
            },
            listen,
        );
        canvas.addEventListener(
            'click',
            () => {
                if (state().kind === 'paused') void resume();
            },
            listen,
        );
        let frame = 0;
        const monitor = () => {
            const game = engine.maybeGame();
            const current = state();
            if (
                started() &&
                game &&
                engine.status().kind !== 'error' &&
                !pending &&
                current.kind !== 'paused'
            ) {
                const live = game._portWebControlsLive();
                if (!live) {
                    liveSince = 0;
                    setState({ kind: game._portWebGetGfxTasks() > 0 ? 'menu' : 'starting' });
                    if (document.pointerLockElement === canvas) releaseFromGame();
                } else if (document.pointerLockElement === canvas) setState({ kind: 'playing' });
                else {
                    liveSince ||= performance.now();
                    if (performance.now() - liveSince > 100) pause('ready');
                }
            }
            frame = requestAnimationFrame(monitor);
        };
        frame = requestAnimationFrame(monitor);
        return () => {
            events.abort();
            cancelAnimationFrame(frame);
            canvas.requestPointerLock = request;
        };
    }
    return { state, notice, attach, pause, resume, starting: () => setState({ kind: 'starting' }) };
}
export type Capture = ReturnType<typeof createCapture>;
