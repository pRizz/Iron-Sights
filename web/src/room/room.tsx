import {
    createEffect,
    createSignal,
    onCleanup,
    onMount,
    Show,
    type Accessor,
    type JSX,
} from 'solid-js';
import type { StartupProgress } from '../startup-progress';
import type { Engine } from '../engine';
import type { RomManager } from '../rom-manager';
import { advanceRoom, roomAction, type RoomFlow } from './flow';
import type { createRoomScene, RoomTarget } from './scene';
import { RomDialog } from './rom-dialog';
import { bindCartridgeDrag } from './pivot';

type Props = {
    returning: boolean;
    engine: Engine;
    rom: RomManager;
    controls: Accessor<boolean>;
    ready: () => boolean;
    progress: Accessor<StartupProgress | undefined>;
    paused: Accessor<boolean>;
    resume: () => Promise<void>;
    openControls: () => void;
    play: () => Promise<void>;
    finish: () => void;
    hint: () => void;
    notify: (message: string) => void;
    unavailable: () => void;
    saveActions: JSX.Element;
};

export function Room(props: Props) {
    const [flow, setFlow] = createSignal<RoomFlow>({
        phase: props.returning ? 'retreating' : 'empty',
        elapsed: 0,
    });
    const [maybeError, setError] = createSignal<string>();
    const [hovered, setHovered] = createSignal('');
    const [selecting, setSelecting] = createSignal(false);
    const [dragging, setDragging] = createSignal(false);
    const [revealing, setRevealing] = createSignal(false);
    const [pivoting, setPivoting] = createSignal(false);
    const [sceneReady, setSceneReady] = createSignal(false);
    let canvas!: HTMLCanvasElement;
    let dialog!: HTMLDialogElement;
    let maybeScene: Awaited<ReturnType<typeof createRoomScene>> | undefined;
    let maybeDrag: ReturnType<typeof bindCartridgeDrag> | undefined;
    let entering = false;
    let handoffReady = false;
    let beginGfx = 0;
    let disposed = false;
    let maybeReportedError: string | undefined;
    const idle = () => ['empty', 'inserted'].includes(flow().phase);
    const hasRom = () => props.rom.status().kind === 'ready';
    const paused = () => !props.returning && props.paused();
    const progress = (): StartupProgress | undefined => {
        if (props.returning) return undefined;
        if (paused()) return { label: 'Startup paused · resume below', maybePercent: null };
        return (
            props.progress() ??
            (props.ready() ? { label: 'Ready to play', maybePercent: 100 } : undefined)
        );
    };
    const label = () => {
        if (selecting() || props.rom.status().kind === 'checking') return 'Checking cartridge';
        const phase = flow().phase;
        if (phase === 'waiting') {
            const status = props.engine.status();
            return status.kind === 'loading' ? status.label : 'Preparing game';
        }
        if (phase === 'inserted') return 'Click the power switch';
        if (phase === 'empty') return 'Insert your cartridge';
        if (phase === 'inserting') return 'Cartridge detected';
        if (phase === 'retreating') return 'Returning to the room';
        return 'Starting GoldenEye';
    };
    createEffect(() => {
        if (props.controls() || flow().phase !== 'empty' || selecting()) maybeDrag?.cancel();
        if (props.controls() && dialog?.open) dialog.close();
        const status = props.rom.status();
        if (status.kind !== 'error') maybeReportedError = undefined;
        if (status.kind === 'error' && !selecting() && status.message !== maybeReportedError) {
            maybeReportedError = status.message;
            setError(status.message);
            if (!props.controls() && dialog && !dialog.open) dialog.showModal();
        }
        if (hasRom() && !selecting() && flow().phase === 'empty')
            setFlow((state) => roomAction(state, 'restore'));
        if (!hasRom() && !selecting() && flow().phase === 'inserted')
            setFlow((state) => roomAction(state, 'reset'));
    });
    createEffect(() => {
        const target = hovered();
        maybeScene?.hover((target || undefined) as RoomTarget | undefined);
    });

    async function select(file: File): Promise<void> {
        if (!idle() || selecting()) return;
        props.hint();
        setSelecting(true);
        setError(undefined);
        await props.rom.select(file);
        if (disposed) return;
        const status = props.rom.status();
        if (status.kind === 'ready') {
            dialog.close();
            setFlow((state) => roomAction(state, 'select'));
        } else if (status.kind === 'error') {
            setError(status.message);
            if (!dialog.open) dialog.showModal();
        }
        setSelecting(false);
    }

    function activate(target: RoomTarget | undefined): void {
        if (target) props.hint();
        if (target === 'controls') {
            props.openControls();
            return;
        }
        if (!idle() || selecting()) return;
        if (target === 'cartridge' && !hasRom()) dialog.showModal();
        if ((target === 'power' || target === 'cartridge') && flow().phase === 'inserted')
            setFlow((state) => roomAction(state, 'activate'));
    }

    async function resumeStartup(): Promise<void> {
        await props.resume();
        // A ready game waits for the cinematic, but a handoff already in
        // progress needs fresh graphics tasks to complete its crossfade.
        if (!disposed && props.ready() && !entering) props.engine.maybeGame()?._portWebSetPaused(1);
    }

    onMount(() => {
        maybeDrag = bindCartridgeDrag(canvas, {
            enabled: () =>
                flow().phase === 'empty' && !props.controls() && !selecting() && !dialog.open,
            hitCartridge: (x, y) => maybeScene?.hit(x, y) === 'cartridge',
            pivot: (dx, dy) => maybeScene?.pivot(dx, dy),
            active: (active) => {
                setPivoting(active);
                maybeScene?.dragging(active);
            },
        });
        const motion = matchMedia('(prefers-reduced-motion: reduce)');
        let frame = 0;
        let previous = performance.now();
        let maybeRevealTimer: ReturnType<typeof setTimeout> | undefined;
        let loadingScene = false;
        canvas.dataset.roomState = 'loading';
        const contextLost = (event: Event) => {
            event.preventDefault();
            if (disposed) return;
            props.notify(
                'The room lost its graphics context. The standard launcher remains available.',
            );
            props.unavailable();
        };
        canvas.addEventListener('webglcontextlost', contextLost);
        const render = (now: number) => {
            const idleScene = ['empty', 'inserted', 'waiting'].includes(flow().phase);
            if ((idleScene || motion.matches || !props.ready()) && now - previous < 1000 / 30) {
                frame = requestAnimationFrame(render);
                return;
            }
            const delta = Math.max(0, Math.min((now - previous) / 1000, 0.1));
            const frameMS = now - previous;
            previous = now;
            // Keep cartridge checking/preparation visible beneath the modal fade.
            if (!props.controls() && !document.hidden && (!dialog.open || selecting())) {
                const next = paused()
                    ? flow()
                    : advanceRoom(flow(), delta, props.ready(), motion.matches);
                setFlow(next);
                try {
                    maybeScene?.render(next, delta, motion.matches, label(), frameMS, progress());
                    setSceneReady(true);
                } catch {
                    props.notify(
                        'The room could not render. The standard launcher remains available.',
                    );
                    props.unavailable();
                    return;
                }
                if (next.phase === 'complete' && !entering && !paused()) {
                    entering = true;
                    beginGfx = props.engine.maybeGame()?._portWebGetGfxTasks() ?? 0;
                    void props.play().then(() => {
                        handoffReady = true;
                    });
                }
                if (
                    handoffReady &&
                    (props.engine.maybeGame()?._portWebGetGfxTasks() ?? 0) > beginGfx + 2
                ) {
                    setRevealing(true);
                    maybeRevealTimer = setTimeout(props.finish, motion.matches ? 180 : 360);
                    return;
                }
            }
            frame = requestAnimationFrame(render);
        };
        onCleanup(() => {
            disposed = true;
            cancelAnimationFrame(frame);
            clearTimeout(maybeRevealTimer);
            maybeDrag?.dispose();
            canvas.removeEventListener('webglcontextlost', contextLost);
            maybeScene?.dispose();
            maybeScene = undefined;
        });
        const loadScene = () => {
            if (loadingScene || maybeScene || disposed) return;
            loadingScene = true;
            void import('./scene')
                .then(async (module) => {
                    if (disposed) return;
                    if (props.controls()) {
                        loadingScene = false;
                        return;
                    }
                    const scene = await module.createRoomScene(canvas);
                    if (disposed) {
                        scene.dispose();
                        return;
                    }
                    maybeScene = scene;
                    canvas.dataset.roomState = 'ready';
                    previous = performance.now();
                    frame = requestAnimationFrame(render);
                })
                .catch(() => {
                    if (disposed) return;
                    canvas.dataset.roomState = 'failed';
                    props.notify(
                        'The 3D room is unavailable. You can use the standard launcher below.',
                    );
                    props.unavailable();
                });
        };
        createEffect(() => {
            if (!props.controls()) loadScene();
        });
    });

    return (
        <div
            class="room-layer"
            hidden={props.controls()}
            data-phase={flow().phase}
            data-revealing={revealing()}
            data-returning={props.returning}
            data-scene-ready={sceneReady()}
            classList={{ 'room-dragging': dragging() }}
            onDragOver={(event) => {
                event.preventDefault();
                if (idle()) setDragging(true);
            }}
            onDragLeave={(event) => {
                if (
                    !event.currentTarget.contains(
                        event.relatedTarget instanceof Node ? event.relatedTarget : null,
                    )
                )
                    setDragging(false);
            }}
            onDrop={(event) => {
                event.preventDefault();
                setDragging(false);
                if (!idle() || dialog.open) return;
                const files = event.dataTransfer?.files;
                if (files?.length === 1) void select(files[0]);
                else {
                    setError('Drop one GoldenEye ROM at a time.');
                    dialog.showModal();
                }
            }}
        >
            <canvas
                ref={canvas}
                class="room-canvas"
                aria-label="A CRT television, Nintendo 64 console and GoldenEye cartridge in a dark room"
                style={{ cursor: pivoting() ? 'grabbing' : hovered() ? 'pointer' : 'default' }}
                onPointerMove={(event) => {
                    if (pivoting()) {
                        setHovered('');
                        return;
                    }
                    const maybeTarget = maybeScene?.hit(event.clientX, event.clientY);
                    setHovered(maybeTarget === 'controls' || idle() ? (maybeTarget ?? '') : '');
                }}
                onPointerLeave={() => setHovered('')}
                onClick={(event) => activate(maybeScene?.hit(event.clientX, event.clientY))}
            />
            <div class="room-keyboard-actions">
                <button
                    disabled={!idle() || selecting()}
                    onClick={() => activate(hasRom() ? 'power' : 'cartridge')}
                >
                    {hasRom() ? 'Power on Nintendo 64' : 'Insert GoldenEye cartridge'}
                </button>
                <button onClick={props.openControls}>Open controls (F2)</button>
            </div>
            <span class="visually-hidden" role="status">
                {progress()?.label ?? label()}
            </span>
            <Show when={idle() && !selecting()}>{props.saveActions}</Show>
            <Show when={paused() && hasRom()}>
                <button class="room-resume" onClick={() => void resumeStartup()}>
                    Resume startup
                </button>
            </Show>
            <RomDialog
                rom={props.rom}
                selecting={selecting}
                maybeError={maybeError}
                controls={props.controls}
                onDialog={(element) => {
                    dialog = element;
                }}
                select={select}
                error={setError}
            />
        </div>
    );
}
