export function pivotRotation(
    yaw: number,
    pitch: number,
    dx: number,
    dy: number,
    width: number,
    height: number,
) {
    return {
        yaw: yaw + (dx / width) * Math.PI * 2,
        pitch: Math.max(-Math.PI / 3, Math.min(Math.PI / 3, pitch + (dy / height) * Math.PI)),
    };
}

type DragCallbacks = {
    enabled: () => boolean;
    hitCartridge: (x: number, y: number) => boolean;
    pivot: (dx: number, dy: number) => void;
    active: (value: boolean) => void;
};

export function bindCartridgeDrag(canvas: HTMLCanvasElement, callbacks: DragCallbacks) {
    let maybeGesture:
        | { id: number; startX: number; startY: number; x: number; y: number; moved: boolean }
        | undefined;
    let suppressClick = false;
    const down = (event: PointerEvent) => {
        suppressClick = false;
        if (
            !event.isPrimary ||
            event.button !== 0 ||
            !callbacks.enabled() ||
            callbacks.hitCartridge(event.clientX, event.clientY)
        )
            return;
        maybeGesture = {
            id: event.pointerId,
            startX: event.clientX,
            startY: event.clientY,
            x: event.clientX,
            y: event.clientY,
            moved: false,
        };
        canvas.setPointerCapture(event.pointerId);
    };
    const move = (event: PointerEvent) => {
        const gesture = maybeGesture;
        if (!gesture || gesture.id !== event.pointerId) return;
        if (!callbacks.enabled()) {
            cancel();
            return;
        }
        if (
            !gesture.moved &&
            Math.hypot(event.clientX - gesture.startX, event.clientY - gesture.startY) < 6
        )
            return;
        gesture.moved = true;
        callbacks.active(true);
        callbacks.pivot(event.clientX - gesture.x, event.clientY - gesture.y);
        gesture.x = event.clientX;
        gesture.y = event.clientY;
    };
    const cancel = () => {
        const gesture = maybeGesture;
        if (!gesture) return;
        suppressClick = gesture.moved;
        maybeGesture = undefined;
        if (canvas.hasPointerCapture(gesture.id)) canvas.releasePointerCapture(gesture.id);
        callbacks.active(false);
    };
    const up = (event: PointerEvent) => {
        if (event.pointerId === maybeGesture?.id) cancel();
    };
    const click = (event: MouseEvent) => {
        if (!suppressClick) return;
        suppressClick = false;
        event.preventDefault();
        event.stopImmediatePropagation();
    };
    const visibility = () => {
        if (document.hidden) cancel();
    };
    canvas.addEventListener('pointerdown', down);
    canvas.addEventListener('pointermove', move);
    canvas.addEventListener('pointerup', up);
    canvas.addEventListener('pointercancel', cancel);
    canvas.addEventListener('lostpointercapture', cancel);
    canvas.addEventListener('click', click, true);
    window.addEventListener('blur', cancel);
    document.addEventListener('visibilitychange', visibility);
    return {
        cancel,
        dispose() {
            cancel();
            canvas.removeEventListener('pointerdown', down);
            canvas.removeEventListener('pointermove', move);
            canvas.removeEventListener('pointerup', up);
            canvas.removeEventListener('pointercancel', cancel);
            canvas.removeEventListener('lostpointercapture', cancel);
            canvas.removeEventListener('click', click, true);
            window.removeEventListener('blur', cancel);
            document.removeEventListener('visibilitychange', visibility);
        },
    };
}
