import type { GameModule } from './runtime-types';

export function diagnosticRing<T>(capacity: number) {
    const values: T[] = [];
    let cursor = 0;
    return {
        add(value: T) {
            values[cursor] = value;
            cursor = (cursor + 1) % capacity;
        },
        read: () =>
            values.length < capacity
                ? values.slice()
                : [...values.slice(cursor), ...values.slice(0, cursor)],
    };
}

// Session-only, bounded metadata: no keys, positions, ROMs, saves or network I/O.
export function createMouseDiagnostics(provenance: () => unknown = () => undefined) {
    // Reuse storage on the high-frequency input path; allocate export objects
    // only when downloading, rather than adding GC pressure to mouse delivery.
    const motion = new Float64Array(1024 * 4);
    let motionCursor = 0,
        motionCount = 0;
    const frames = diagnosticRing<object>(512);
    let mode = () => 'unavailable';
    let maybeCanvas: HTMLCanvasElement | undefined;
    let lastFrame = 0;
    let lastState = '';
    let cssWidth = 0,
        cssHeight = 0;
    return {
        attach(canvas: HTMLCanvasElement, requestMode: () => string) {
            maybeCanvas = canvas;
            mode = requestMode;
            const observer = new ResizeObserver((entries) => {
                cssWidth = entries[0].contentRect.width;
                cssHeight = entries[0].contentRect.height;
            });
            observer.observe(canvas);
            const move = (event: MouseEvent) => {
                if (document.pointerLockElement !== canvas) return;
                const offset = motionCursor * 4;
                motion[offset] = performance.now();
                motion[offset + 1] = event.timeStamp;
                motion[offset + 2] = event.movementX;
                motion[offset + 3] = event.movementY;
                motionCursor = (motionCursor + 1) % 1024;
                motionCount = Math.min(1024, motionCount + 1);
            };
            canvas.addEventListener('mousemove', move, { capture: true, passive: true });
            return () => {
                canvas.removeEventListener('mousemove', move, true);
                observer.disconnect();
                maybeCanvas = undefined;
            };
        },
        sample(maybeGame: GameModule | undefined, state: string) {
            if (!maybeGame || (state === 'paused' && lastState === 'paused')) return;
            const now = performance.now();
            if (state === lastState && now - lastFrame < 16) return;
            frames.add({
                time: now,
                gap: lastFrame && lastState !== 'paused' ? now - lastFrame : 0,
                state,
                locked: document.pointerLockElement === maybeCanvas,
                mode: mode(),
                width: maybeCanvas?.width,
                height: maybeCanvas?.height,
                cssWidth,
                cssHeight,
                pumps: maybeGame?._portWebGetFrames?.(),
                heading: state === 'playing' ? maybeGame?._portWebGetPlayerHeading?.() : undefined,
            });
            lastFrame = now;
            lastState = state;
        },
        download() {
            const box = maybeCanvas?.getBoundingClientRect();
            const report = {
                schema: 1,
                capturedAt: new Date().toISOString(),
                browser: navigator.userAgent,
                requestMode: mode(),
                runtime: provenance(),
                viewport: { width: innerWidth, height: innerHeight, dpr: devicePixelRatio },
                canvas: {
                    width: maybeCanvas?.width,
                    height: maybeCanvas?.height,
                    cssWidth: box?.width,
                    cssHeight: box?.height,
                },
                motion: Array.from({ length: motionCount }, (_, index) => {
                    const offset = ((motionCount < 1024 ? index : motionCursor + index) % 1024) * 4;
                    return {
                        arrival: motion[offset],
                        timestamp: motion[offset + 1],
                        x: motion[offset + 2],
                        y: motion[offset + 3],
                    };
                }),
                frames: frames.read(),
            };
            const url = URL.createObjectURL(
                new Blob([JSON.stringify(report, null, 2)], { type: 'application/json' }),
            );
            const link = document.createElement('a');
            link.href = url;
            link.download = 'goldeneye-mouse-diagnostics.json';
            link.click();
            setTimeout(() => URL.revokeObjectURL(url), 1000);
        },
    };
}
