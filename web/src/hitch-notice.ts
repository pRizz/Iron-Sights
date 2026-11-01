// Work only with animation-frame timestamps; a warning is not a diagnosis.
export function createHitchNotice(onHitch: () => void) {
    let maybePrevious: number | undefined;
    let maybeActiveSince: number | undefined;
    let maybeFirstGap: number | undefined;
    let warned = false;

    return (now: number, active: boolean): void => {
        if (!active) {
            maybePrevious = undefined;
            maybeActiveSince = undefined;
            maybeFirstGap = undefined;
            return;
        }
        if (maybePrevious === undefined) {
            maybePrevious = now;
            maybeActiveSince = now;
            return;
        }

        const previous = maybePrevious;
        const gap = now - previous;
        maybePrevious = now;
        if (warned || now - (maybeActiveSince ?? now) <= 5000 || previous === maybeActiveSince)
            return;
        if (gap >= 1000) {
            warned = true;
            onHitch();
            return;
        }
        if (gap < 500) return;
        if (maybeFirstGap === undefined || now - maybeFirstGap > 60000) {
            maybeFirstGap = now;
            return;
        }
        warned = true;
        onHitch();
    };
}
