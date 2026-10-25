import { createMemo, createSignal, onMount, onCleanup, Show, type Accessor } from 'solid-js';
import { ShimmerButton } from 'mystic-ui';
import type { Engine } from './engine';
import type { RomManager } from './rom-manager';
import type { Capture } from './capture';
import { observeGameSize } from './presentation';
import { TouchControls } from './touch-controls';
import type { TouchInput } from './touch-input';
import { observeTouchViewport } from './touch-viewport';

type PlayerProps = {
    engine: Engine;
    rom: RomManager;
    capture: Capture;
    touch: TouchInput;
    touchVisible: Accessor<boolean>;
    openControls: () => void;
    started: Accessor<boolean>;
    presenting: Accessor<boolean>;
    resume: () => Promise<void>;
    ready: () => boolean;
    onCanvas: (canvas: HTMLCanvasElement) => void;
    chooseROM: () => void;
    fullscreen: () => Promise<void>;
    play: () => Promise<void>;
    notify: (message: string) => void;
};

export function Player(props: PlayerProps) {
    const { engine, rom, capture, started, ready } = props;
    const [dragging, setDragging] = createSignal(false);
    let canvas!: HTMLCanvasElement;
    let stage!: HTMLDivElement;
    onMount(() => onCleanup(observeGameSize(canvas, stage)));
    onMount(() => onCleanup(observeTouchViewport(stage)));
    const maybeError = createMemo(() => {
        const value = engine.status();
        return value.kind === 'error' ? value : undefined;
    });
    const maybeRomError = createMemo(() => {
        const value = rom.status();
        return value.kind === 'error' ? value : undefined;
    });
    const maybePercent = createMemo(() => {
        const value = engine.status();
        return value.kind === 'loading' ? value.maybePercent : null;
    });
    const launchLabel = () => {
        const maybeStartup = engine.maybeStartup();
        if (maybeStartup) return maybeStartup.label;
        const status = engine.status();
        if (status.kind === 'loading') return status.label;
        if (status.kind === 'error') return 'Game unavailable';
        if (rom.status().kind === 'checking') return 'Checking ROM';
        if (started()) return 'Escape releases the mouse. P opens the watch.';
        return ready() ? 'All set. Start your mission.' : 'Choose your ROM to begin.';
    };
    const playbackLabel = () =>
        ({
            stopped: 'Browser preview',
            starting: 'Starting game',
            menu: 'Mouse free',
            playing: 'Playing',
            paused: 'Paused',
            capturing: 'Capturing mouse',
            'mouse-free': 'Mouse free',
        })[capture.state().kind];
    return (
        <section class="player-card" aria-label="Game player">
            <div class="player-toolbar">
                <span class="status-dot" />
                <span>{playbackLabel()}</span>
                <span class="toolbar-spacer" />
                <button
                    class="quiet"
                    disabled={!started()}
                    onClick={capture.downloadMouseDiagnostics}
                >
                    Download mouse diagnostics
                </button>
                <button class="quiet" disabled={!started()} onClick={() => capture.pause('manual')}>
                    Pause
                </button>
                <button
                    class="quiet"
                    disabled={!document.fullscreenEnabled}
                    onClick={() => void props.fullscreen()}
                >
                    Fullscreen
                </button>
            </div>
            <div
                ref={stage}
                class="game-stage"
                classList={{ dragging: dragging(), 'touch-mode': props.touch.enabled() }}
                onDragOver={(event) => {
                    event.preventDefault();
                    if (!started()) setDragging(true);
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
                    if (started()) return;
                    const maybeTransfer = event.dataTransfer;
                    if (!maybeTransfer || maybeTransfer.files.length !== 1) {
                        props.notify('Drop one ROM at a time.');
                        return;
                    }
                    void rom.select(maybeTransfer.files[0]);
                }}
            >
                <canvas
                    id="canvas"
                    ref={(element) => {
                        canvas = element;
                        props.onCanvas(element);
                    }}
                    width="960"
                    height="540"
                    tabIndex={started() ? 0 : -1}
                    aria-label="Project Midas game canvas"
                />
                <Show when={started() && props.touch.enabled()}>
                    <TouchControls
                        engine={engine}
                        capture={capture}
                        input={props.touch}
                        visible={props.touchVisible}
                        options={props.openControls}
                    />
                </Show>
                <Show when={!started()}>
                    <div class="welcome-overlay">
                        <div class="reticle" aria-hidden="true">
                            <i />
                            <span>007</span>
                        </div>
                        <p class="eyebrow">LOCAL GAME DATA / NO UPLOAD</p>
                        <h2>
                            {rom.status().kind === 'ready'
                                ? 'Your mission is ready.'
                                : 'Bring your GoldenEye ROM.'}
                        </h2>
                        <Show
                            when={rom.status().kind === 'ready'}
                            fallback={
                                <>
                                    <p>Drop it here, or choose a file to get started.</p>
                                    <button class="file-button" onClick={() => props.chooseROM()}>
                                        Choose ROM <span aria-hidden="true">↗</span>
                                    </button>
                                </>
                            }
                        >
                            <p>Your ROM is loaded. Start when you're ready.</p>
                            <button
                                class="file-button"
                                disabled={!ready()}
                                onClick={() => void props.play()}
                            >
                                Play <span aria-hidden="true">→</span>
                            </button>
                        </Show>
                        <small>Original US release · 12 MiB · .z64 / .v64 / .n64</small>
                    </div>
                </Show>
                <Show when={started() && props.presenting() && capture.state().kind === 'paused'}>
                    <div class="pause-overlay">
                        <p class="eyebrow">GAME PAUSED</p>
                        <h2>Ready to continue?</h2>
                        <p>
                            {props.touch.enabled()
                                ? 'Resume to return to touch controls.'
                                : 'Resume to return to the game and capture your mouse.'}
                        </p>
                        <ShimmerButton
                            class="primary"
                            background="#d9b775"
                            shimmerColor="#fff0c4"
                            borderRadius="8px"
                            disabled={capture.state().kind === 'capturing'}
                            onClick={() => void props.resume()}
                        >
                            Resume game
                        </ShimmerButton>
                        <Show when={capture.notice()}>
                            <p role="alert">{capture.notice()}</p>
                        </Show>
                    </div>
                </Show>
                <Show
                    when={started() && props.presenting() && capture.state().kind === 'mouse-free'}
                >
                    <button
                        class="mouse-capture-hint"
                        onPointerDown={(event) => event.preventDefault()}
                        onClick={() => void props.resume()}
                    >
                        Click to capture the mouse · keyboard controls are ready
                    </button>
                </Show>
            </div>
            <div class="launch-bar" aria-live="polite">
                <div>
                    <strong>{launchLabel()}</strong>
                    <Show when={maybePercent() !== null}>
                        <progress
                            aria-label="Game download"
                            max="100"
                            value={maybePercent() ?? 0}
                        />
                    </Show>
                </div>
                <Show when={!started()}>
                    <ShimmerButton
                        class="primary"
                        background="#d9b775"
                        shimmerColor="#fff0c4"
                        borderRadius="8px"
                        disabled={!ready()}
                        onClick={() => void props.play()}
                    >
                        Play GoldenEye <span aria-hidden="true">→</span>
                    </ShimmerButton>
                </Show>
            </div>
            <Show when={maybeError()}>
                {(error) => (
                    <div class="error-panel" role="alert">
                        <p>{error().message}</p>
                        <button
                            onClick={() =>
                                error().needsReload
                                    ? location.reload()
                                    : void engine.initialize(canvas)
                            }
                        >
                            Try again
                        </button>
                    </div>
                )}
            </Show>
            <Show when={maybeRomError()}>
                {(error) => (
                    <p class="error-panel" role="alert">
                        {error().message}
                    </p>
                )}
            </Show>
        </section>
    );
}
