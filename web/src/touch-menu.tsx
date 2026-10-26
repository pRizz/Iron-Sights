import { onCleanup, onMount } from 'solid-js';
import { createTouchContacts } from './touch-contacts';
import { menuTapMoved, type TouchInput } from './touch-input';

export function TouchMenu(props: { canvas: () => HTMLCanvasElement; input: TouchInput }) {
    let maybePan: { id: number; x: number; y: number; moved: boolean } | undefined;
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
    const contacts = createTouchContacts((id, maybeEvent) => {
        if (maybePan?.id !== id) return;
        const pan = maybePan;
        maybePan = undefined;
        if (!maybeEvent || maybeEvent.type !== 'pointerup') return;
        position(maybeEvent);
        if (pan.moved || menuTapMoved(maybeEvent.clientX, maybeEvent.clientY, pan.x, pan.y)) return;
        props.input.state.press(id, 'accept');
        props.input.state.release(id);
    });
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
            aria-label="Drag menu cursor; tap to select"
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
                };
                position(event);
            }}
            onPointerMove={(event) => {
                if (maybePan?.id !== event.pointerId) return;
                maybePan.moved ||= menuTapMoved(
                    event.clientX,
                    event.clientY,
                    maybePan.x,
                    maybePan.y,
                );
                position(event);
            }}
            onContextMenu={(event) => event.preventDefault()}
        />
    );
}
