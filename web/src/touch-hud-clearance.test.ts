import { expect, test } from 'bun:test';
import {
    CLEARANCE_GAP,
    cornerLift,
    createCornerClearance,
    fractionBox,
    isPortrait,
    unionBox,
} from './touch-hud-clearance';

const box = (left: number, top: number, right: number, bottom: number) => ({
    left,
    top,
    right,
    bottom,
});
const ipad = box(0, 0, 1024, 768);

test('corner controls are measured as the union of their drawn boxes', () => {
    // R spans the N64 cluster's top, Z its right edge; an empty box is skipped.
    expect(unionBox([box(788, 286, 988, 347), box(890, 357, 988, 728), box(5, 5, 5, 9)])).toEqual(
        box(788, 286, 988, 728),
    );
    expect(unionBox([])).toBeUndefined();
});

test('the reported ammo box lands on the canvas, wherever it sits', () => {
    expect(fractionBox([0.75, 0.85, 0.9, 0.97], ipad)).toEqual(box(768, 691.2, 870.4, 744.96));
    expect(fractionBox([0.25, 0.5, 0.25, 0.5], box(50, 0, 450, 400))).toEqual(
        box(150, 100, 250, 200),
    );
    // -1 is the runtime's "nothing drawn yet"
    expect(fractionBox([-1, -1, -1, -1], ipad)).toBeUndefined();
});

test('controls over the ammo rise until their bottom clears its top', () => {
    // iPad: the cluster rests 40 px above the bottom, over the ammo's 74 px row.
    const controls = box(788, 286, 988, 728);
    const ammo = box(760, 694, 880, 750);
    const lift = cornerLift(controls, ammo);
    expect(controls.bottom - lift).toBeLessThanOrEqual(ammo.top - CLEARANCE_GAP);
    expect(lift).toBe(Math.ceil(728 - (694 - CLEARANCE_GAP)));
});

test('ammo beside or below the controls, or none, leaves them in place', () => {
    const controls = box(788, 286, 988, 728);
    // a phone's 4:3 band keeps the ammo left of the cluster
    expect(cornerLift(controls, box(600, 694, 788, 750))).toBe(0);
    expect(cornerLift(controls, box(700, 740, 900, 760))).toBe(0);
    expect(cornerLift(controls, undefined)).toBe(0);
    expect(cornerLift(undefined, box(760, 694, 880, 750))).toBe(0);
});

test('a window taller than wide is portrait', () => {
    expect(isPortrait(390, 844)).toBe(true); // iPhone upright
    expect(isPortrait(844, 390)).toBe(false);
    expect(isPortrait(800, 800)).toBe(false);
});

// A stand-in overlay whose right-hand controls sit over the ammo the game
// reports, as on a tall phone held upright.
function clearanceFixture() {
    const style = new Map<string, string>();
    const root = {
        style: {
            setProperty: (name: string, value: string) => void style.set(name, value),
            getPropertyValue: (name: string) => style.get(name) ?? '',
        },
        querySelectorAll: (selector: string) =>
            selector.includes('touch-actions')
                ? [{ getBoundingClientRect: () => box(250, 500, 380, 640) }]
                : [],
    } as unknown as HTMLElement;
    const canvas = { getBoundingClientRect: () => box(0, 300, 390, 592) } as unknown as Element;
    const covered: number[] = [];
    const ammo = [0.7, 0.95, 0.85, 0.98]; // right side's box on the canvas
    const game = {
        _portWebTouchHudCorner: (side: number, on: number) => {
            covered[side] = on;
        },
        _portWebTouchAmmoBox: (side: number, edge: number) => (side === 1 ? (ammo[edge] ?? -1) : -1),
    };
    return { style, root, canvas, covered, game };
}

test('landscape lifts the right-hand controls above the ammo', () => {
    const { style, root, canvas, covered, game } = clearanceFixture();
    createCornerClearance().update(game, root, canvas);
    expect(parseFloat(style.get('--touch-lift-right') ?? '0')).toBeGreaterThan(0);
    expect(covered[1]).toBe(1);
});

test('portrait keeps the corner controls at rest and covers nothing', () => {
    const { style, root, canvas, covered, game } = clearanceFixture();
    const clearance = createCornerClearance();
    clearance.update(game, root, canvas);
    clearance.remeasure(); // the phone turns upright
    clearance.update(game, root, canvas, true);
    expect(style.get('--touch-lift-right')).toBe('0px');
    expect(covered[1]).toBe(0);
});