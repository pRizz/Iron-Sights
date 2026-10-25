import { expect, test } from 'bun:test';
import { cameraPose, insertionPose, smooth } from './cinematic';

test('cartridge aligns above the slot before descending straight into it', () => {
    expect(insertionPose(0, 2)).toEqual({ position: [-0.2, 1.35, 1.65], yaw: 2 });
    expect(insertionPose(0.55, 2)).toEqual({ position: [-0.2, 1.22, 0.98], yaw: 0 });
    const halfway = insertionPose(0.8, 2);
    expect(halfway.position[2]).toBe(0.98);
    expect(halfway.position[1]).toBeGreaterThan(0.66);
    expect(insertionPose(1, 2).position[1]).toBeCloseTo(0.66);
});
test('camera approaches the screen with bounded endpoints and zero endpoint velocity', () => {
    const start = cameraPose(0, 16 / 9);
    const end = cameraPose(1, 16 / 9);
    expect(start.position).toEqual([2.6, 2.4, 6.3]);
    expect(end.position).toEqual([-0.06, 2.1, -0.45]);
    expect(cameraPose(2, 16 / 9)).toEqual(end);
    expect(smooth(0.001)).toBeLessThan(0.000001);
    expect(1 - smooth(0.999)).toBeLessThan(0.000001);
});
test('portrait framing backs away without changing the destination', () => {
    expect(cameraPose(0, 0.7).position[2]).toBeGreaterThan(cameraPose(0, 1.7).position[2]);
    expect(cameraPose(1, 0.7)).toEqual(cameraPose(1, 1.7));
});
