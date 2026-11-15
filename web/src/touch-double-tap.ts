// Double-Tap Fire (FORK-079): a quick, still tap on the look area arms a
// short window; a second touch that lands near it inside the window fires.
// The first tap never fires, so a stray single tap is harmless.

export type TapPoint = { x: number; y: number; time: number };

export const DOUBLE_TAP = {
    /** Longest press that still counts as the first tap. */
    tapMs: 250,
    /** Farthest the first tap may wander, in CSS pixels (the button tap slop). */
    tapSlop: 10,
    /** Longest gap from the first tap's release to the second touch. */
    gapMs: 300,
    /** Farthest the second touch may land from the first tap's release. */
    reach: 48,
} as const;

/** The first contact of a pair: short and still, so not a look drag. */
export function isTap(start: TapPoint, end: TapPoint, wandered: number): boolean {
    return end.time - start.time <= DOUBLE_TAP.tapMs && wandered <= DOUBLE_TAP.tapSlop;
}

/** The second contact of a pair: soon after and near the first tap's release. */
export function isSecondTap(tap: TapPoint, touch: TapPoint): boolean {
    const gap = touch.time - tap.time;
    return (
        gap >= 0 &&
        gap <= DOUBLE_TAP.gapMs &&
        Math.hypot(touch.x - tap.x, touch.y - tap.y) <= DOUBLE_TAP.reach
    );
}

// Remembers the last tap; each new look touch either completes the pair
// (and consumes it) or clears it, so three quick taps fire only once.
export function createDoubleTap() {
    let maybeTap: TapPoint | undefined;
    return {
        /** A look contact begins: true when it completes a double tap. */
        begin(touch: TapPoint): boolean {
            const second = maybeTap !== undefined && isSecondTap(maybeTap, touch);
            maybeTap = undefined;
            return second;
        },
        /** A look contact ended normally; a firing contact never arms the next pair. */
        end(start: TapPoint, end: TapPoint, wandered: number, fired: boolean) {
            maybeTap = !fired && isTap(start, end, wandered) ? end : undefined;
        },
        clear() {
            maybeTap = undefined;
        },
    };
}
