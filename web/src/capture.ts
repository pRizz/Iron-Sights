import { createSignal } from 'solid-js';
import type { Engine } from './engine';
import { installMouseLock } from './mouse-lock';
import { createMouseDiagnostics } from './mouse-diagnostics';

export type Playback =
    | { kind: 'stopped' | 'starting' | 'menu' | 'playing' | 'capturing' | 'mouse-free' }
    | { kind: 'paused'; reason: 'focus' | 'mouse' | 'manual' };

export function createCapture(
    engine: Engine,
    started: () => boolean,
    touch: () => boolean = () => false,
) {
    const [state, setState] = createSignal<Playback>({ kind: 'stopped' });
    const [notice, setNotice] = createSignal('');
    let maybeCanvas: HTMLCanvasElement | undefined;
    let intentional = false;
    let wasLocked = false;
    let pending = false;
    const diagnostics = createMouseDiagnostics(() => engine.maybeManifest?.());
    let maybeLock: ReturnType<typeof installMouseLock> | undefined;
    let maybeCaptureFailure: ((error: unknown) => void) | undefined;
    let requestResume = () => {
        void resume();
    };

    function releaseFromGame(): void {
        if (document.pointerLockElement !== maybeCanvas) return;
        intentional = true;
        document.exitPointerLock();
    }
    engine.onPointerRelease(releaseFromGame);

    function pause(reason: 'focus' | 'mouse' | 'manual'): void {
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
        const wasPaused = state().kind === 'paused';
        setNotice('');
        canvas.focus();
        try {
            game._portWebSetPaused(0);
            if (touch()) {
                setState({ kind: game._portWebControlsLive() ? 'playing' : 'menu' });
                return;
            }
            if (!game._portWebControlsLive()) {
                setState({ kind: 'menu' });
                return;
            }
            setState({ kind: 'capturing' });
            await new Promise<void>((resolve, reject) => {
                const done = (maybeError?: Error) => {
                    clearTimeout(timer);
                    document.removeEventListener('pointerlockchange', changed);
                    document.removeEventListener('pointerlockerror', lockError);
                    maybeCaptureFailure = undefined;
                    maybeError ? reject(maybeError) : resolve();
                };
                const changed = () => {
                    if (document.pointerLockElement === canvas) done();
                };
                const failed = () =>
                    done(new Error('Mouse capture was refused. Click Resume to try again.'));
                const lockError = () => {
                    if (!maybeLock?.pending()) failed();
                };
                const timer = setTimeout(failed, 5000);
                document.addEventListener('pointerlockchange', changed);
                document.addEventListener('pointerlockerror', lockError);
                maybeCaptureFailure = failed;
                if (game._portWebCaptureMouse() < 0) failed();
            });
            const current = state();
            if (current.kind === 'paused') {
                pause(current.reason);
                return;
            }
            if (document.hidden || !document.hasFocus()) {
                pause('focus');
                return;
            }
            setState({ kind: 'playing' });
        } catch (error) {
            setNotice(
                error instanceof Error ? error.message : 'Mouse capture failed. Try Resume again.',
            );
            const current = state();
            if (current.kind === 'paused') pause(current.reason);
            else if (document.hidden || !document.hasFocus()) pause('focus');
            else if (wasPaused) pause('mouse');
            else setState({ kind: 'mouse-free' });
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
        const lock = installMouseLock(canvas, (error) => maybeCaptureFailure?.(error));
        maybeLock = lock;
        const detachDiagnostics = diagnostics.attach(canvas, lock.mode);
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
                if (state().kind === 'paused' || state().kind === 'mouse-free') requestResume();
            },
            listen,
        );
        canvas.addEventListener(
            'keydown',
            (event) => {
                if (
                    event.repeat ||
                    event.altKey ||
                    event.ctrlKey ||
                    event.metaKey ||
                    ['Escape', 'F2', 'Enter', 'p', 'P'].includes(event.key)
                )
                    return;
                if (state().kind === 'mouse-free') requestResume();
            },
            listen,
        );
        let frame = 0;
        const monitor = () => {
            const game = engine.maybeGame();
            const current = state();
            diagnostics.sample(started() ? game : undefined, current.kind);
            if (
                started() &&
                game &&
                engine.status().kind !== 'error' &&
                !pending &&
                current.kind !== 'paused'
            ) {
                const live = game._portWebControlsLive();
                if (!live) {
                    setState({ kind: game._portWebGetGfxTasks() > 0 ? 'menu' : 'starting' });
                    if (document.pointerLockElement === canvas) releaseFromGame();
                } else if (touch() || document.pointerLockElement === canvas)
                    setState({ kind: 'playing' });
                else {
                    // Arriving from a cutscene/watch is not focus loss. If the
                    // browser wants a capture gesture, keep keyboard play live.
                    setState({ kind: 'mouse-free' });
                }
            }
            frame = requestAnimationFrame(monitor);
        };
        frame = requestAnimationFrame(monitor);
        return () => {
            events.abort();
            cancelAnimationFrame(frame);
            lock.dispose();
            detachDiagnostics();
            maybeLock = undefined;
        };
    }
    return {
        state,
        notice,
        downloadMouseDiagnostics: diagnostics.download,
        attach,
        pause,
        resume,
        onResumeRequest: (callback: () => void) => {
            requestResume = callback;
        },
        starting: () => setState({ kind: 'starting' }),
    };
}
export type Capture = ReturnType<typeof createCapture>;
