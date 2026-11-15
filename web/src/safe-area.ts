// The canvas fills the stage under an iPhone's notch and rounded corners; the
// game moves edge-pinned text (the FPS counter) clear of the safe area instead.

export type Insets = { left: number; top: number; right: number; bottom: number };
export type Box = { left: number; top: number; width: number; height: number };

const NONE: Insets = { left: 0, top: 0, right: 0, bottom: 0 };

function fraction(overlap: number, size: number) {
    return size > 0 && Number.isFinite(overlap) && overlap > 0 ? Math.min(overlap / size, 0.5) : 0;
}

// How far each viewport safe-area inset reaches into the canvas, as a fraction
// of the canvas width (left, right) or height (top, bottom).
export function canvasSafeArea(insets: Insets, canvas: Box, viewport: Box): Insets {
    if (canvas.width <= 0 || canvas.height <= 0) return NONE;
    const safeRight = viewport.left + viewport.width - insets.right;
    const safeBottom = viewport.top + viewport.height - insets.bottom;
    return {
        left: fraction(viewport.left + insets.left - canvas.left, canvas.width),
        top: fraction(viewport.top + insets.top - canvas.top, canvas.height),
        right: fraction(canvas.left + canvas.width - safeRight, canvas.width),
        bottom: fraction(canvas.top + canvas.height - safeBottom, canvas.height),
    };
}

export type SafeAreaOwner = (left: number, top: number, right: number, bottom: number) => void;

// Reads env(safe-area-inset-*) through a hidden probe and reports the canvas's
// share to the game whenever the canvas, the window or the orientation moves.
export function observeSafeArea(
    canvas: HTMLCanvasElement,
    maybeOwner: () => SafeAreaOwner | undefined,
) {
    const probe = document.createElement('div');
    probe.setAttribute('aria-hidden', 'true');
    probe.style.cssText =
        'position:fixed;inset:0;visibility:hidden;pointer-events:none;' +
        'padding:env(safe-area-inset-top,0px) env(safe-area-inset-right,0px) ' +
        'env(safe-area-inset-bottom,0px) env(safe-area-inset-left,0px)';
    document.body.append(probe);
    let frame = 0;
    let last = '';
    const update = () => {
        frame = 0;
        const owner = maybeOwner();
        if (!owner) {
            last = '';
            return;
        }
        const style = getComputedStyle(probe);
        const rect = canvas.getBoundingClientRect();
        const area = canvasSafeArea(
            {
                left: parseFloat(style.paddingLeft) || 0,
                top: parseFloat(style.paddingTop) || 0,
                right: parseFloat(style.paddingRight) || 0,
                bottom: parseFloat(style.paddingBottom) || 0,
            },
            rect,
            { left: 0, top: 0, width: innerWidth, height: innerHeight },
        );
        const key = `${area.left},${area.top},${area.right},${area.bottom}`;
        if (key === last) return;
        last = key;
        owner(area.left, area.top, area.right, area.bottom);
    };
    const schedule = () => {
        if (!frame) frame = requestAnimationFrame(update);
    };
    const resize = new ResizeObserver(schedule);
    resize.observe(canvas);
    resize.observe(probe);
    window.addEventListener('resize', schedule);
    window.addEventListener('orientationchange', schedule);
    schedule();
    return {
        // Report again, for example once the game starts and can take it.
        refresh: () => {
            last = '';
            schedule();
        },
        stop: () => {
            resize.disconnect();
            window.removeEventListener('resize', schedule);
            window.removeEventListener('orientationchange', schedule);
            cancelAnimationFrame(frame);
            probe.remove();
        },
    };
}
