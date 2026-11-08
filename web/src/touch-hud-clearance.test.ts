import { expect, test } from 'bun:test';
import { CLEARANCE_GAP, cornerLift, fractionBox, unionBox } from './touch-hud-clearance';

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
