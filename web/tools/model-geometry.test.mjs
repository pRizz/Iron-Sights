import { expect, test } from 'bun:test';
import * as THREE from 'three';
import { partitionGeometry } from './model-geometry.mjs';

test('power-switch splitting preserves normalized vertex colors and triangle ownership', () => {
    // Arrange: one triangle on either side, with packed linear color attributes.
    const source = new THREE.BufferGeometry();
    source.setAttribute(
        'position',
        new THREE.Float32BufferAttribute(
            [-2, 0, 0, -1, 0, 0, -1, 1, 0, 1, 0, 0, 2, 0, 0, 2, 1, 0],
            3,
        ),
    );
    source.setAttribute(
        'color',
        new THREE.Uint8BufferAttribute(
            Array.from({ length: 6 }, () => [255, 128, 0]).flat(),
            3,
            true,
        ),
    );
    source.setIndex([0, 1, 2, 3, 4, 5]);
    // Act
    const [stationary, moving] = partitionGeometry(source, (point) => point.x > 0);
    // Assert
    expect(stationary.attributes.position.count).toBe(3);
    expect(moving.attributes.position.count).toBe(3);
    expect(moving.attributes.position.getX(0)).toBe(1);
    expect(stationary.attributes.position.getX(0)).toBe(-2);
    expect(moving.attributes.color.normalized).toBe(true);
    expect(moving.attributes.color.getX(0)).toBe(1);
    expect(moving.attributes.color.getY(0)).toBeCloseTo(128 / 255);
    expect(source.index.count).toBe(6);
});
