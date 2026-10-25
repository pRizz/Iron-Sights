import { createEffect, createSignal, onCleanup, onMount, type Accessor } from 'solid-js';
import type { Engine } from '../engine';
import type { RomManager } from '../rom-manager';
import { advanceRoom, roomAction, type RoomFlow } from './flow';
import type { createRoomScene, RoomTarget } from './scene';
import { RomDialog } from './rom-dialog';

type Props = {
    engine: Engine;
    rom: RomManager;
    controls: Accessor<boolean>;
    ready: () => boolean;
    openControls: () => void;
    play: () => Promise<void>;
    finish: () => void;
    hint: () => void;
    notify: (message: string) => void;
    unavailable: () => void;
};

export function Room(props: Props) {
    const [flow, setFlow] = createSignal<RoomFlow>({ phase: 'empty', elapsed: 0 });
    const [maybeError, setError] = createSignal<string>();
    const [hovered, setHovered] = createSignal('');
    const [selecting, setSelecting] = createSignal(false);
    const [dragging, setDragging] = createSignal(false);
    const [revealing, setRevealing] = createSignal(false);
    let canvas!: HTMLCanvasElement;
    let dialog!: HTMLDialogElement;
    let maybeScene: ReturnType<typeof createRoomScene> | undefined;
    let entering = false;
    let disposed = false;
    let maybeReportedError: string | undefined;
    const idle = () => ['empty', 'inserted'].includes(flow().phase);
    const hasRom = () => props.rom.status().kind === 'ready';
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
        return 'Starting GoldenEye';
    };
    createEffect(() => {
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

    onMount(() => {
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
            if ((idleScene || motion.matches) && now - previous < 1000 / 30) {
                frame = requestAnimationFrame(render);
                return;
            }
            const delta = Math.max(0, Math.min((now - previous) / 1000, 0.1));
            const frameMS = now - previous;
            previous = now;
            if (!props.controls() && !document.hidden && !dialog.open) {
                const next = advanceRoom(flow(), delta, props.ready(), motion.matches);
                setFlow(next);
                try {
                    maybeScene?.render(next, delta, motion.matches, label(), frameMS);
                } catch {
                    props.notify(
                        'The room could not render. The standard launcher remains available.',
                    );
                    props.unavailable();
                    return;
                }
                if (next.phase === 'complete' && !entering) {
                    entering = true;
                    void props.play();
                }
                if (entering && (props.engine.maybeGame()?._portWebGetGfxTasks() ?? 0) > 2) {
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
            canvas.removeEventListener('webglcontextlost', contextLost);
            maybeScene?.dispose();
            maybeScene = undefined;
        });
        const loadScene = () => {
            if (loadingScene || maybeScene || disposed) return;
            loadingScene = true;
            void import('./scene')
                .then((module) => {
                    if (disposed) return;
                    if (props.controls()) {
                        loadingScene = false;
                        return;
                    }
                    maybeScene = module.createRoomScene(canvas);
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
                style={{ cursor: hovered() ? 'pointer' : 'default' }}
                onPointerMove={(event) => {
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
                {label()}
            </span>
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
