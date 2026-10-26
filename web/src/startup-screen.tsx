import {
    createEffect,
    createMemo,
    createSignal,
    onCleanup,
    onMount,
    Show,
    type Accessor,
} from 'solid-js';
import type { Engine } from './engine';
import type { StartupProgress } from './startup-progress';

type Props = {
    engine: Engine;
    progress: Accessor<StartupProgress | undefined>;
    retry: () => void;
    showControls: () => void;
    paused: Accessor<boolean>;
    resume: () => void;
};

function StartupError(props: {
    message: string;
    blocked: boolean;
    retry: () => void;
    dismiss: () => void;
}) {
    let dialog!: HTMLDialogElement;
    let disposed = false;
    onMount(() => {
        dialog.showModal();
        onCleanup(() => {
            disposed = true;
            dialog.close();
        });
    });
    return (
        <dialog
            ref={dialog}
            class="startup-error"
            onClose={() => {
                if (!disposed) props.dismiss();
            }}
            aria-labelledby="startup-error-title"
        >
            <p class="eyebrow">STARTUP PAUSED</p>
            <h2 id="startup-error-title">
                {props.blocked ? 'Your game is open in another tab' : 'Unable to start GoldenEye'}
            </h2>
            <p>
                {props.blocked
                    ? 'Close the other game tab, then try again here. Your saves are protected.'
                    : props.message}
            </p>
            <div class="startup-actions">
                <button autofocus class="primary" onClick={props.retry}>
                    Try again
                </button>
                <button onClick={() => dialog.close()}>Open controls</button>
            </div>
        </dialog>
    );
}

export function StartupScreen(props: Props) {
    const [dismissed, setDismissed] = createSignal(false);
    const maybeFailure = createMemo(() => {
        const status = props.engine.status();
        return status.kind === 'error' ? status : undefined;
    });
    createEffect(() => {
        if (!maybeFailure()) setDismissed(false);
    });
    return (
        <>
            <Show when={props.progress()}>
                {(progress) => (
                    <section
                        class="startup-screen"
                        role="status"
                        aria-live="polite"
                        aria-busy="true"
                    >
                        <div class="startup-content">
                            <span class="startup-mark" aria-hidden="true">
                                007
                            </span>
                            <p class="eyebrow">GOLDENEYE · PROJECT MIDAS ADS</p>
                            <p class="startup-attribution">A fork of Project Midas</p>
                            <h1>{props.paused() ? 'Startup paused' : progress().label}</h1>
                            <Show
                                when={props.paused()}
                                fallback={
                                    <>
                                        <Show
                                            when={progress().maybePercent !== null}
                                            fallback={
                                                <progress aria-label={progress().label} max="100" />
                                            }
                                        >
                                            <progress
                                                aria-label={progress().label}
                                                max="100"
                                                value={progress().maybePercent ?? 0}
                                            />
                                        </Show>
                                        <p class="startup-measure">
                                            {progress().maybePercent === null
                                                ? 'Please wait'
                                                : `${progress().maybePercent}%`}
                                        </p>
                                    </>
                                }
                            >
                                <div class="startup-actions">
                                    <button class="primary" onClick={props.resume}>
                                        Resume startup
                                    </button>
                                </div>
                            </Show>
                            <p class="startup-help">
                                Your ROM and saves stay on this device. <span>F2 · Controls</span>
                            </p>
                            <Show when={props.engine.maybeManifest()}>
                                {(manifest) => (
                                    <p class="startup-attribution">
                                        Built at: {manifest().build.built_at}
                                    </p>
                                )}
                            </Show>
                        </div>
                    </section>
                )}
            </Show>
            <Show when={!dismissed() && maybeFailure()}>
                {(failure) => (
                    <StartupError
                        message={failure().message}
                        blocked={failure().reason === 'other-tab'}
                        retry={props.retry}
                        dismiss={() => {
                            setDismissed(true);
                            props.showControls();
                        }}
                    />
                )}
            </Show>
        </>
    );
}
