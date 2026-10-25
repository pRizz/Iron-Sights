import { expect, test } from 'bun:test';
import { pivotRotation } from './pivot';

test('background drag rotates proportionally to the viewport', () => {
    expect(pivotRotation(0, 0, 250, 0, 1000, 600).yaw).toBeCloseTo(Math.PI / 2);
    expect(pivotRotation(1, 0, -250, 0, 1000, 600).yaw).toBeCloseTo(1 - Math.PI / 2);
});
test('vertical dragging clamps tilt without flipping the cartridge', () => {
    expect(pivotRotation(0, 0, 0, 5000, 1000, 600).pitch).toBe(Math.PI / 3);
    expect(pivotRotation(0, 0, 0, -5000, 1000, 600).pitch).toBe(-Math.PI / 3);
});
