import {
    createEffect,
    createSignal,
    onCleanup,
    onMount,
    Show,
    For,
    type Accessor,
    type JSX,
} from 'solid-js';
import type { Engine } from './engine';
import type { Capture } from './capture';
import { stickVector, type TouchAction, type TouchInput } from './touch-input';
import { createTouchContacts, suppressTouchGestures } from './touch-contacts';
import { TouchMenu } from './touch-menu';
import './touch.css';

type Props = {
    engine: Engine;
    capture: Capture;
    input: TouchInput;
    visible: Accessor<boolean>;
    canvas: () => HTMLCanvasElement;
};
const extra: [TouchAction, string][] = [
    ['previousWeapon', 'Weapon −'],
    ['nextWeapon', 'Weapon +'],
    ['previousGadget', 'Gadget −'],
    ['nextGadget', 'Gadget +'],
    ['zoomIn', 'Zoom +'],
    ['zoomOut', 'Zoom −'],
    ['detonate', 'Detonate'],
    ['sprint', 'Sprint'],
];

export function TouchControls(props: Props) {
    const [menu, setMenu] = createSignal(true);
    const [files, setFiles] = createSignal(false);
    const [more, setMore] = createSignal(false);
    const [aiming, setAiming] = createSignal(false);
    const [stick, setStick] = createSignal({ x: 0, y: 0 });
    let maybeMove: number | undefined;
    let maybeLook: { id: number; x: number; y: number } | undefined;
    let maybeFireLook: { id: number; x: number; y: number } | undefined;
    const active = () =>
        props.visible() && props.input.enabled() && props.capture.state().kind !== 'paused';
    const contacts = createTouchContacts((id) => {
        props.input.state.release(id);
        if (maybeMove === id) {
            maybeMove = undefined;
            setStick({ x: 0, y: 0 });
            props.input.move(0, 0);
        }
        if (maybeLook?.id === id) maybeLook = undefined;
        if (maybeFireLook?.id === id) maybeFireLook = undefined;
    });
    const clear = () => {
        contacts.clear();
        maybeMove = undefined;
        maybeLook = undefined;
        maybeFireLook = undefined;
        setAiming(false);
        setStick({ x: 0, y: 0 });
        setMore(false);
        props.input.reset();
    };
    createEffect(() => {
        if (!active()) clear();
    });
    createEffect(() => {
        props.input.layout();
        clear();
    });
    onMount(() => {
        const detachContacts = contacts.attach();
        let frame = 0;
        const monitor = () => {
            const nextFiles = !!props.engine.maybeGame()?._portWebFileMenuMetric?.(0);
            if (nextFiles !== files()) {
                clear();
                setFiles(nextFiles);
            }
            const next = !props.engine.maybeGame()?._portWebControlsLive();
            if (next !== menu()) {
                clear();
                setMenu(next);
            }
            frame = requestAnimationFrame(monitor);
        };
        frame = requestAnimationFrame(monitor);
        window.addEventListener('blur', clear);
        window.addEventListener('orientationchange', clear);
        window.addEventListener('pagehide', clear);
        const hidden = () => {
            if (document.hidden) clear();
        };
        document.addEventListener('visibilitychange', hidden);
        onCleanup(() => {
            detachContacts();
            cancelAnimationFrame(frame);
            window.removeEventListener('blur', clear);
            window.removeEventListener('orientationchange', clear);
            window.removeEventListener('pagehide', clear);
            document.removeEventListener('visibilitychange', hidden);
            clear();
        });
    });
    const begin = (event: PointerEvent) => {
        props.input.gesture();
        event.preventDefault();
        event.stopPropagation();
        contacts.begin(event);
    };
    const TapButton = (button: { label: string; activate: () => void; expanded?: boolean }) => (
        <button
            type="button"
            aria-expanded={button.expanded}
            onPointerDown={(event) => {
                begin(event);
                button.activate();
            }}
            onClick={(event) => {
                if (event.detail === 0) button.activate();
            }}
        >
            {button.label}
        </button>
    );
    const Button = (button: {
        action: TouchAction;
        label: string;
        children?: JSX.Element;
        maybeController?: 'a' | 'b' | 'z' | 'start';
    }) => (
        <button
            type="button"
            data-touch-action={button.action}
            data-controller-button={button.maybeController}
            aria-label={button.label}
            aria-pressed={button.action === 'aim' ? aiming() : undefined}
            onContextMenu={(event) => event.preventDefault()}
            onPointerDown={(event) => {
                begin(event);
                if (button.action === 'aim') {
                    if (aiming()) props.input.state.release(-1);
                    else props.input.state.press(-1, 'aim');
                    setAiming(!aiming());
                } else props.input.state.press(event.pointerId, button.action);
                if (button.action === 'fire')
                    maybeFireLook = { id: event.pointerId, x: event.clientX, y: event.clientY };
            }}
            onPointerMove={(event) => {
                if (maybeFireLook?.id !== event.pointerId) return;
                const scale = 120 / Math.min(innerWidth, innerHeight);
                props.input.look(
                    (event.clientX - maybeFireLook.x) * scale,
                    (event.clientY - maybeFireLook.y) * scale,
                );
                maybeFireLook = { id: event.pointerId, x: event.clientX, y: event.clientY };
            }}
        >
            {button.children ?? button.label}
        </button>
    );
    const move = (event: PointerEvent) => {
        if (maybeMove !== event.pointerId) return;
        const box = (event.currentTarget as HTMLElement).getBoundingClientRect();
        const vector = stickVector(
            event.clientX - box.x - box.width / 2,
            event.clientY - box.y - box.height / 2,
            box.width * 0.4,
        );
        setStick(vector);
        props.input.move(vector.x, vector.y);
    };
    return (
        <Show when={active()}>
            <div
                ref={(surface) => onCleanup(suppressTouchGestures(surface))}
                class="touch-controls"
                aria-label="Touch game controls"
                data-mode={menu() ? 'menu' : 'game'}
                data-layout={props.input.layout()}
                data-file-screen={files() ? 'true' : undefined}
                onContextMenu={(event) => event.preventDefault()}
            >
                <Show when={props.input.layout() === 'modern'}>
                    <div class="touch-top">
                        <Button action="watch" label="Watch" />
                    </div>
                </Show>
                <Show
                    when={!menu()}
                    fallback={
                        <>
                            <TouchMenu canvas={props.canvas} input={props.input} />
                            <div class="touch-dpad">
                                <Button action="up" label="Up" />
                                <Button action="left" label="Left" />
                                <Button action="down" label="Down" />
                                <Button action="right" label="Right" />
                            </div>
                            <Show when={props.input.layout() === 'modern' && !files()}>
                                <div class="touch-menu-actions">
                                    <Button action="accept" label="Select" />
                                    <Button action="back" label="Back" />
                                </div>
                            </Show>
                            <span class="touch-menu-hint">
                                Drag to move cursor · Tap to select · Or use arrows
                            </span>
                        </>
                    }
                >
                    <div
                        class="touch-look"
                        aria-label="Drag to look"
                        onContextMenu={(event) => event.preventDefault()}
                        onPointerDown={(event) => {
                            begin(event);
                            if (!maybeLook)
                                maybeLook = {
                                    id: event.pointerId,
                                    x: event.clientX,
                                    y: event.clientY,
                                };
                        }}
                        onPointerMove={(event) => {
                            if (maybeLook?.id !== event.pointerId) return;
                            const scale = 120 / Math.min(innerWidth, innerHeight);
                            props.input.look(
                                (event.clientX - maybeLook.x) * scale,
                                (event.clientY - maybeLook.y) * scale,
                            );
                            maybeLook = { id: event.pointerId, x: event.clientX, y: event.clientY };
                        }}
                    >
                        <span>Drag to look</span>
                    </div>
                    <div
                        class="touch-stick"
                        role="group"
                        aria-label="Move thumbstick"
                        onPointerDown={(event) => {
                            begin(event);
                            if (maybeMove === undefined) {
                                maybeMove = event.pointerId;
                                move(event);
                            }
                        }}
                        onPointerMove={move}
                    >
                        <span>MOVE</span>
                        <i
                            style={{
                                transform: `translate(${stick().x * 38}px, ${-stick().y * 38}px)`,
                            }}
                        />
                    </div>
                    <div class="touch-weapon">
                        <Button action="nextWeapon" label="Switch weapon">
                            <svg
                                viewBox="0 0 32 32"
                                width="28"
                                height="28"
                                aria-hidden="true"
                                fill="none"
                                stroke="currentColor"
                                stroke-width="2"
                                stroke-linejoin="round"
                            >
                                <path d="M4 8h24v7H16l-3 11H6l3-11H4z" />
                                <path d="M16 15v5h5l2-5M7 11h17" />
                            </svg>
                        </Button>
                    </div>
                    <Show when={props.input.layout() === 'modern'}>
                        <div class="touch-actions">
                            <Button action="aim" label="Aim" />
                            <Button action="fire" label="Fire" />
                            <Button action="reload" label="Reload" />
                            <Button action="use" label="Use" />
                            <Button action="crouch" label="Crouch" />
                            <TapButton
                                label="More"
                                expanded={more()}
                                activate={() => setMore(!more())}
                            />
                        </div>
                        <Show when={more()}>
                            <div class="touch-extra">
                                <For each={extra}>
                                    {([action, label]) => <Button action={action} label={label} />}
                                </For>
                            </div>
                        </Show>
                    </Show>
                </Show>
                <Show when={props.input.layout() === 'n64' && !files()}>
                    <div class="touch-n64" role="group" aria-label="N64 controller buttons">
                        <Show when={!menu()}>
                            <Button action="aim" label="Aim" />
                        </Show>
                        <Button action="watch" label="N64 Start" maybeController="start">
                            Start
                        </Button>
                        <Button action="back" label="N64 B" maybeController="b">
                            B
                        </Button>
                        <Button action="accept" label="N64 A" maybeController="a">
                            A
                        </Button>
                        <Button action="fire" label="N64 Z" maybeController="z">
                            Z
                        </Button>
                    </div>
                </Show>
            </div>
        </Show>
    );
}
