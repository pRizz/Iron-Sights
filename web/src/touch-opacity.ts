// Touch Controls Opacity (FORK-080): the gameplay controls show at the
// player's chosen strength while resting; a control under a finger shows at
// TOUCH_PRESSED_OPACITY or the resting strength, whichever is more.

/** Strength of a touched control, as a fraction of its designed look. */
export const TOUCH_PRESSED_OPACITY = 0.8;

/** Strength before the game reports its setting (the setting's default). */
export const TOUCH_DEFAULT_OPACITY = 0.5;

/** The [display] touch_controls_opacity percent as a CSS opacity. */
export function touchRestOpacity(maybePercent: number | undefined): number {
    if (maybePercent === undefined || !Number.isFinite(maybePercent) || maybePercent <= 0) {
        return TOUCH_DEFAULT_OPACITY;
    }
    return Math.min(1, maybePercent / 100);
}

/** Selector for the controls that fade at rest and brighten while touched. */
export const TOUCH_FADING_CONTROLS = 'button, .touch-stick';

type Released = { pointerId: number };

/**
 * Marks the control each finger is on with data-touching until that finger
 * lifts or is cancelled; a control stays marked while any finger holds it.
 */
export function createTouchingTracker() {
    const holding = new Map<number, HTMLElement>();
    const release = (event: Released) => {
        const maybeControl = holding.get(event.pointerId);
        if (maybeControl === undefined) return;
        holding.delete(event.pointerId);
        if (![...holding.values()].includes(maybeControl)) delete maybeControl.dataset.touching;
    };
    return {
        press(pointerId: number, maybeControl: HTMLElement | null) {
            if (maybeControl === null) return;
            const previous = holding.get(pointerId);
            if (previous !== undefined) release({ pointerId });
            holding.set(pointerId, maybeControl);
            maybeControl.dataset.touching = 'true';
        },
        release,
        clear() {
            for (const control of holding.values()) delete control.dataset.touching;
            holding.clear();
        },
    };
}

/**
 * The overlay's side of the setting: update() puts the resting strength on
 * the overlay root as --touch-rest-opacity (touch.css applies it), and
 * attach() marks the control under each finger until it lifts.
 */
export function createTouchFade() {
    const touching = createTouchingTracker();
    let maybeShown: number | undefined;
    return {
        update(maybeRoot: HTMLElement | undefined, maybePercent: number | undefined) {
            const opacity = touchRestOpacity(maybePercent);
            if (maybeRoot === undefined || opacity === maybeShown) return;
            maybeShown = opacity;
            maybeRoot.style.setProperty('--touch-rest-opacity', String(opacity));
        },
        attach(root: HTMLElement) {
            const pressed = (event: PointerEvent) => {
                const maybeControl =
                    event.target instanceof Element
                        ? event.target.closest<HTMLElement>(TOUCH_FADING_CONTROLS)
                        : null;
                touching.press(event.pointerId, root.contains(maybeControl) ? maybeControl : null);
            };
            root.addEventListener('pointerdown', pressed, true);
            window.addEventListener('pointerup', touching.release);
            window.addEventListener('pointercancel', touching.release);
            return () => {
                root.removeEventListener('pointerdown', pressed, true);
                window.removeEventListener('pointerup', touching.release);
                window.removeEventListener('pointercancel', touching.release);
            };
        },
        clear: touching.clear,
    };
}
