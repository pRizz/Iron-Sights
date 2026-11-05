import { onCleanup, onMount } from 'solid-js';
import { createTouchContacts } from './touch-contacts';
import { createValueScrub, menuTapMoved, type TouchInput } from './touch-input';

// About five native frames: long enough for the cursor to arrive first.
const HOLD_DELAY_MS = 80;

export function TouchMenu(props: {
    canvas: () => HTMLCanvasElement;
    input: TouchInput;
    adjust?: boolean;
    hold?: boolean;
}) {
    let maybePan: { id: number; x: number; y: number; moved: boolean; held: boolean } | undefined;
    // On a hold-to-set screen (the 007 sliders) A is held while the finger rests,
    // starting after the cursor has reached it so the native screen picks the
    // touched row before A locks it.
    let maybeHoldTimer: ReturnType<typeof setTimeout> | undefined;
    const stopHoldTimer = () => {
        clearTimeout(maybeHoldTimer);
        maybeHoldTimer = undefined;
    };
    // While a slider edit is open the native cursor is pinned; a drag steps its value.
    const scrub = createValueScrub();
    const position = (event: PointerEvent) => {
        // SDL receives cursor motion only; it must never see a touch-down mouse
        // press. Select goes through the existing native A-button pulse on tap-up.
        props.canvas().dispatchEvent(
            new MouseEvent('mousemove', {
                bubbles: true,
                clientX: event.clientX,
                clientY: event.clientY,
            }),
        );
    };
    const contacts = createTouchContacts(
        (id, maybeEvent) => {
            if (maybePan?.id !== id) return;
            const pan = maybePan;
            maybePan = undefined;
            stopHoldTimer();
            if (maybeEvent?.type === 'pointerup') position(maybeEvent);
            if (pan.held) {
                props.input.state.release(id);
                return;
            }
            if (!maybeEvent || maybeEvent.type !== 'pointerup') return;
            if (pan.moved || menuTapMoved(maybeEvent.clientX, maybeEvent.clientY, pan.x, pan.y))
                return;
            props.input.state.press(id, 'accept');
            props.input.state.release(id);
        },
        (id, event) => {
            if (maybePan?.id !== id) return;
            maybePan.moved ||= menuTapMoved(event.clientX, event.clientY, maybePan.x, maybePan.y);
            position(event);
            const steps = props.adjust ? scrub.move(event.clientX) : 0;
            if (steps !== 0) props.input.step(steps);
        },
    );
    onMount(() => {
        const detach = contacts.attach();
        const clear = () => contacts.clear();
        const hidden = () => {
            if (document.hidden) clear();
        };
        window.addEventListener('blur', clear);
        window.addEventListener('orientationchange', clear);
        window.addEventListener('pagehide', clear);
        document.addEventListener('visibilitychange', hidden);
        onCleanup(() => {
            stopHoldTimer();
            detach();
            window.removeEventListener('blur', clear);
            window.removeEventListener('orientationchange', clear);
            window.removeEventListener('pagehide', clear);
            document.removeEventListener('visibilitychange', hidden);
        });
    });
    return (
        <div
            class="touch-menu-surface"
            aria-label={
                props.adjust
                    ? 'Drag sideways to change the value; tap to keep it'
                    : props.hold
                      ? 'Touch and drag along a bar to set it; tap to select'
                      : 'Drag menu cursor; tap to select'
            }
            onPointerDown={(event) => {
                event.preventDefault();
                event.stopPropagation();
                props.input.gesture();
                if (maybePan && maybePan.id !== event.pointerId) return;
                contacts.begin(event);
                maybePan = {
                    id: event.pointerId,
                    x: event.clientX,
                    y: event.clientY,
                    moved: false,
                    held: false,
                };
                scrub.begin(event.clientX);
                position(event);
                if (!props.hold) return;
                const id = event.pointerId;
                stopHoldTimer();
                maybeHoldTimer = setTimeout(() => {
                    maybeHoldTimer = undefined;
                    if (maybePan?.id !== id) return;
                    maybePan.held = true;
                    props.input.state.press(id, 'accept');
                }, HOLD_DELAY_MS);
            }}
            onContextMenu={(event) => event.preventDefault()}
        />
    );
}
