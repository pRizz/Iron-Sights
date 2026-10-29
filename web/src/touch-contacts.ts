// Capture is best effort on mobile: terminal events can target the document or
// disappear during an OS gesture. Keep ownership independent of DOM bubbling.
export function createTouchContacts(
    ended: (id: number, maybeEvent?: PointerEvent) => void,
    maybeMoved?: (id: number, event: PointerEvent, target: HTMLElement) => void,
) {
    const contacts = new Map<number, { target: HTMLElement; type: string }>();
    const end = (id: number, maybeEvent?: PointerEvent) => {
        const maybeContact = contacts.get(id);
        if (!maybeContact) return;
        contacts.delete(id);
        ended(id, maybeEvent);
        if (maybeContact.target.hasPointerCapture(id))
            maybeContact.target.releasePointerCapture(id);
    };
    const clear = () => {
        for (const id of [...contacts.keys()]) end(id);
    };
    return {
        clear,
        begin(event: PointerEvent) {
            const target = event.currentTarget as HTMLElement;
            for (const [id, contact] of contacts) {
                if (contact.target === target && !target.hasPointerCapture(id)) end(id);
            }
            // A fresh down with a reused ID supersedes a missed release.
            end(event.pointerId);
            contacts.set(event.pointerId, { target, type: event.pointerType });
            try {
                target.setPointerCapture(event.pointerId);
            } catch {
                // A cancelled native contact can race the down handler. Document
                // terminal events and the touch-stream fallback still own cleanup.
            }
        },
        attach() {
            const events = new AbortController();
            const options = { capture: true, signal: events.signal };
            const finish = (event: PointerEvent) => end(event.pointerId, event);
            document.addEventListener('pointerup', finish, options);
            document.addEventListener('pointercancel', finish, options);
            document.addEventListener(
                'pointermove',
                (event) => {
                    const maybeContact = contacts.get(event.pointerId);
                    if (maybeContact) maybeMoved?.(event.pointerId, event, maybeContact.target);
                },
                options,
            );
            document.addEventListener(
                'lostpointercapture',
                (event) => {
                    if (contacts.get(event.pointerId)?.target === event.target) finish(event);
                },
                options,
            );
            const reconcile = (event: TouchEvent) => {
                if (event.touches.length) return;
                // iOS can end its touch stream without a matching pointer event.
                for (const [id, contact] of contacts) {
                    if (contact.type === 'touch') end(id);
                }
            };
            document.addEventListener('touchend', reconcile, options);
            document.addEventListener('touchcancel', reconcile, options);
            return () => {
                events.abort();
                clear();
            };
        },
    };
}

// PointerEvent.preventDefault alone does not cancel Safari's touch gestures.
// Scope the non-passive fallback to the controls, preserving launcher zoom.
export function suppressTouchGestures(surface: HTMLElement): () => void {
    const events = new AbortController();
    const prevent = (event: Event) => {
        if (event.cancelable) event.preventDefault();
    };
    for (const type of ['touchstart', 'touchmove', 'touchend', 'gesturestart', 'gesturechange']) {
        surface.addEventListener(type, prevent, { passive: false, signal: events.signal });
    }
    return () => events.abort();
}
