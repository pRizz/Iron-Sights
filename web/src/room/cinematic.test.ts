import { expect, test } from 'bun:test';
import { cameraPose, insertionPose, smooth } from './cinematic';
import { PerspectiveCamera, Vector3 } from 'three';
import { ROOM_FOV, TV_SCREEN } from './framing';

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
    expect(end.position[2]).toBeGreaterThan(TV_SCREEN.center[2] + 1);
    expect(cameraPose(2, 16 / 9)).toEqual(end);
    expect(smooth(0.001)).toBeLessThan(0.000001);
    expect(1 - smooth(0.999)).toBeLessThan(0.000001);
});
test('portrait framing backs away to contain the complete TV screen', () => {
    expect(cameraPose(0, 0.7).position[2]).toBeGreaterThan(cameraPose(0, 1.7).position[2]);
    expect(cameraPose(1, 0.7).position[2]).toBeGreaterThan(cameraPose(1, 1.7).position[2]);
});

for (const aspect of [0.5, 4 / 3, 16 / 9, 21 / 9]) {
    test(`TV screen corners fit the final canvas at aspect ${aspect}`, () => {
        // Arrange
        const camera = new PerspectiveCamera(ROOM_FOV, aspect, 0.05, 35);
        const pose = cameraPose(1, aspect);
        camera.position.set(...pose.position);
        camera.lookAt(...pose.look);
        camera.updateMatrixWorld();
        const center = new Vector3(...TV_SCREEN.center);
        // Act
        const corners = [-1, 1].flatMap((x) =>
            [-1, 1].map((y) =>
                center
                    .clone()
                    .add(new Vector3((x * TV_SCREEN.width) / 2, (y * TV_SCREEN.height) / 2, 0))
                    .project(camera),
            ),
        );
        // Assert: neither axis crops, and at least one reaches the canvas edge.
        const edges = corners.flatMap((point) => [Math.abs(point.x), Math.abs(point.y)]);
        expect(Math.max(...edges)).toBeCloseTo(1, 6);
        expect(edges.every((edge) => edge <= 1.000001)).toBe(true);
    });
}
