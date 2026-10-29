export type TouchContext = 'navigation' | 'cinematic' | 'game' | 'watch';

// Values match WebTouchContext in port/web/touch.h. Unknown/older runtimes
// keep the HUD hidden rather than assuming that gameplay has started.
export function touchContext(value: unknown): TouchContext {
    if (value === 0) return 'navigation';
    if (value === 2) return 'game';
    if (value === 3) return 'watch';
    return 'cinematic';
}
