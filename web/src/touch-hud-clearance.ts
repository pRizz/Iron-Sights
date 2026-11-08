// Corner ammo stays where the game draws it; the gameplay buttons over each
// bottom corner rise above it where they would cover it, as on a 4:3 iPad
// whose HUD reaches the corners (FORK-028).
export type ClearanceBox = Pick<DOMRectReadOnly, 'left' | 'right' | 'top' | 'bottom'>;

// Sides match gfx_hud_touch_corner (port/fast3d/gfx_api.h); each lift is a CSS
// variable added to its corner controls' bottom offset in touch.css.
export const clearanceCorners = [
    { side: 0, selector: '.touch-stick, .touch-left-actions', lift: '--touch-lift-left' },
    // The More tray sits above the actions, so they decide the right corner.
    { side: 1, selector: '.touch-n64, .touch-actions', lift: '--touch-lift-right' },
] as const;
// CSS pixels between the top of the ammo and the controls above it.
export const CLEARANCE_GAP = 8;
// Frames after a layout change in which a lift may also fall: the ammo box of
// the old layout is replaced by the next frame the game draws.
export const CLEARANCE_SETTLE_FRAMES = 30;

// The union of the drawn boxes, or none.
export function unionBox(boxes: readonly ClearanceBox[]): ClearanceBox | undefined {
    const drawn = boxes.filter((box) => box.right > box.left && box.bottom > box.top);
    if (drawn.length === 0) return undefined;
    return {
        left: Math.min(...drawn.map((box) => box.left)),
        right: Math.max(...drawn.map((box) => box.right)),
        top: Math.min(...drawn.map((box) => box.top)),
        bottom: Math.max(...drawn.map((box) => box.bottom)),
    };
}

// A box of canvas fractions (left, right, top, bottom) placed on the canvas.
export function fractionBox(
    fractions: readonly number[],
    canvas: ClearanceBox,
): ClearanceBox | undefined {
    const [left = -1, right = -1, top = -1, bottom = -1] = fractions;
    if (!(right > left) || !(bottom > top) || left < 0 || top < 0) return undefined;
    const width = canvas.right - canvas.left;
    const height = canvas.bottom - canvas.top;
    return {
        left: canvas.left + left * width,
        right: canvas.left + right * width,
        top: canvas.top + top * height,
        bottom: canvas.top + bottom * height,
    };
}

// How far controls at rest must rise so their bottom clears the ammo's top;
// 0 when the ammo is beside them or there is none.
export function cornerLift(
    controls: ClearanceBox | undefined,
    ammo: ClearanceBox | undefined,
): number {
    if (!controls || !ammo) return 0;
    if (ammo.right <= controls.left || ammo.left >= controls.right) return 0;
    return Math.max(0, Math.ceil(controls.bottom - (ammo.top - CLEARANCE_GAP)));
}

type ClearanceGame = {
    _portWebTouchHudCorner?(side: number, covered: number): void;
    _portWebTouchAmmoBox?(side: number, edge: number): number;
};

// Per frame: tells the runtime which corners gameplay controls cover and lifts
// those controls above the ammo it reports. Lifts only grow once settled, so a
// weapon whose icon is shorter, or a hidden counter, never drops the buttons.
export function createCornerClearance() {
    let maybeGame: ClearanceGame | undefined;
    let settle = 0;
    let canvasBox: ClearanceBox = { left: 0, right: 0, top: 0, bottom: 0 };
    const rest: (ClearanceBox | undefined)[] = [undefined, undefined];
    const lifts = [0, 0];
    const covered = [false, false];
    const cover = (game: ClearanceGame, side: number, on: boolean) => {
        if (covered[side] === on) return;
        covered[side] = on;
        game._portWebTouchHudCorner?.(side, Number(on));
    };
    return {
        // A layout or viewport change: controls are measured again at rest.
        remeasure() {
            settle = 0;
        },
        // maybeRoot is the overlay during gameplay or the watch, else none.
        update(game: ClearanceGame, maybeRoot: HTMLElement | undefined, canvas: Element) {
            if (game !== maybeGame) {
                maybeGame = game;
                covered.fill(false);
                settle = 0;
            }
            if (maybeRoot && settle === 0) canvasBox = canvas.getBoundingClientRect();
            for (const { side, selector, lift } of clearanceCorners) {
                if (!maybeRoot) {
                    lifts[side] = 0;
                    cover(game, side, false);
                    continue;
                }
                if (settle === 0) {
                    // Measured lifted, the rest position is that much lower.
                    const current = parseFloat(maybeRoot.style.getPropertyValue(lift)) || 0;
                    const lifted = unionBox(
                        [...maybeRoot.querySelectorAll(selector)].map((element) =>
                            element.getBoundingClientRect(),
                        ),
                    );
                    rest[side] = lifted && {
                        ...lifted,
                        top: lifted.top + current,
                        bottom: lifted.bottom + current,
                    };
                }
                cover(game, side, !!rest[side]);
                const edges = [0, 1, 2, 3].map(
                    (edge) => game._portWebTouchAmmoBox?.(side, edge) ?? -1,
                );
                const needed = cornerLift(rest[side], fractionBox(edges, canvasBox));
                const next =
                    settle < CLEARANCE_SETTLE_FRAMES ? needed : Math.max(lifts[side], needed);
                if (next !== lifts[side] || settle === 0)
                    maybeRoot.style.setProperty(lift, `${next}px`);
                lifts[side] = next;
            }
            settle = maybeRoot ? Math.min(settle + 1, CLEARANCE_SETTLE_FRAMES) : 0;
        },
    };
}
