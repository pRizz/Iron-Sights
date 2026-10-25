import { expect, test } from 'bun:test';
import * as THREE from 'three';
import { batchStaticProps } from './batch';

test('batching preserves static world positions and an interactive switch', () => {
    const scene = new THREE.Scene();
    const parent = new THREE.Group();
    parent.position.x = 3;
    scene.add(parent);
    const material = new THREE.MeshStandardMaterial();
    const fixed = new THREE.Mesh(new THREE.BoxGeometry(), material);
    const second = new THREE.Mesh(new THREE.BoxGeometry(), material.clone());
    second.position.x = 2;
    const moving = new THREE.Mesh(new THREE.BoxGeometry(), material);
    parent.add(fixed, second, moving);
    batchStaticProps(scene, [moving]);
    expect(moving.parent).toBe(parent);
    const batch = scene.children.find((object) => object instanceof THREE.Mesh) as THREE.Mesh;
    batch.geometry.computeBoundingBox();
    expect(batch.geometry.boundingBox!.min.x).toBe(2.5);
    expect(batch.geometry.boundingBox!.max.x).toBe(5.5);
});

test('emissive LEDs remain independently adjustable', () => {
    const scene = new THREE.Scene();
    const material = new THREE.MeshStandardMaterial({ emissive: 0xff0000 });
    const led = new THREE.Mesh(new THREE.BoxGeometry(), material);
    const other = new THREE.Mesh(new THREE.BoxGeometry(), material);
    scene.add(led, other);
    batchStaticProps(scene, []);
    material.emissiveIntensity = 2;
    expect(scene.children).toEqual([led, other]);
});
