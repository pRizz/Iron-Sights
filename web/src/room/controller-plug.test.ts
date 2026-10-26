import { expect, test } from 'bun:test';
import { readFile } from 'node:fs/promises';
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { createControllerPlug } from './controller-plug';

test('plug seats on the licensed model socket rim without a gap or center offset', async () => {
    // Arrange: measure the actual shipped model, independently of the plug geometry.
    const bytes = await readFile(new URL('../../public/room/console.glb', import.meta.url));
    const model = (
        await new GLTFLoader().parseAsync(
            bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
            '',
        )
    ).scene;
    model.updateMatrixWorld(true);
    const rim: THREE.Vector3[] = [];
    model.traverse((object) => {
        if (!(object instanceof THREE.Mesh)) return;
        const positions = object.geometry.attributes.position;
        for (let i = 0; i < positions.count; i++) {
            const point = new THREE.Vector3()
                .fromBufferAttribute(positions, i)
                .applyMatrix4(object.matrixWorld);
            if (
                Math.abs(point.z - 0.09205979) < 1e-7 &&
                Math.abs(Math.hypot(point.x + 0.066, point.y - 0.033) - 0.012431) < 1e-6
            )
                rim.push(point);
        }
    });
    expect(rim.length).toBeGreaterThanOrEqual(12);
    // Act
    createControllerPlug(model);
    model.updateMatrixWorld(true);
    const shell = model.getObjectByName('PlugShell') as THREE.Mesh;
    const positions = shell.geometry.attributes.position;
    const shellPoints = Array.from({ length: positions.count }, (_, i) =>
        new THREE.Vector3().fromBufferAttribute(positions, i).applyMatrix4(shell.matrixWorld),
    );
    // Assert: each socket-rim vertex meets the plug's full-radius seating ring.
    for (const point of rim)
        expect(
            Math.min(...shellPoints.map((candidate) => point.distanceTo(candidate))),
        ).toBeLessThan(4e-6);
});

test('sculpted plug has finite smooth normals and a continuous transformed cable exit', () => {
    // Arrange
    const model = new THREE.Group();
    model.position.set(-0.2, 0.04, 1.22);
    model.scale.setScalar(6);
    // Act
    const { exit, lead } = createControllerPlug(model);
    const plug = model.getObjectByName('ControllerPlug')!;
    const shell = model.getObjectByName('PlugShell') as THREE.Mesh;
    const normal = shell.geometry.attributes.normal;
    // Assert: deformation must not create invalid lighting, including the seam/caps.
    for (let i = 0; i < normal.count; i++) {
        const value = new THREE.Vector3().fromBufferAttribute(normal, i);
        expect(Number.isFinite(value.length())).toBe(true);
        expect(value.length()).toBeCloseTo(1, 5);
    }
    const localExit = plug.worldToLocal(new THREE.Vector3(...exit));
    const localLead = plug.worldToLocal(new THREE.Vector3(...lead));
    expect(localExit.x).toBeCloseTo(0, 8);
    expect(localExit.y).toBeCloseTo(0, 8);
    expect(localLead.z).toBeGreaterThan(localExit.z);
    expect(localLead.x).toBeCloseTo(localExit.x, 8);
    expect(localLead.y).toBeCloseTo(localExit.y, 8);
    const relief = model.getObjectByName('PlugStrainRelief') as THREE.Mesh;
    relief.geometry.computeBoundingBox();
    expect(localExit.z).toBeLessThan(relief.position.z + relief.geometry.boundingBox!.max.z);
});
