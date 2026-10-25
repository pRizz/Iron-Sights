import * as THREE from 'three';
import { box, canvasTexture, plastic } from './geometry';
import { extrudedProfile, lettering, texturedPlastic } from './materials';
import { loadCartridgeArtwork } from './artwork';

export function createCartridge(scene: THREE.Scene) {
    const group = new THREE.Group();
    const shape = new THREE.Shape();
    shape.moveTo(-0.43, -0.27);
    shape.lineTo(0.43, -0.27);
    shape.lineTo(0.43, 0.15);
    shape.quadraticCurveTo(0.36, 0.25, 0, 0.28);
    shape.quadraticCurveTo(-0.36, 0.25, -0.43, 0.15);
    shape.closePath();
    const body = extrudedProfile(shape, 0.1, texturedPlastic(0xa6aaa6, 0.6), 0.015);
    body.position.z = -0.06;
    group.add(body);
    box(group, [0.83, 0.013, 0.13], [0, -0.24, -0.01], plastic(0x6b706d), 0.004);
    box(group, [0.7, 0.02, 0.085], [0, -0.278, -0.01], plastic(0x292c2b), 0.004);
    for (const x of [-0.36, 0.36]) {
        for (let index = 0; index < 5; index++)
            box(
                group,
                [0.055, 0.012, 0.004],
                [x, -0.04 + index * 0.033, 0.059],
                plastic(0x919791),
                0.002,
            );
        const screw = new THREE.Mesh(
            new THREE.CylinderGeometry(0.018, 0.018, 0.005, 12),
            plastic(0x5c625f),
        );
        screw.rotation.x = Math.PI / 2;
        screw.position.set(x, -0.18, -0.067);
        group.add(screw);
    }
    const artwork = loadCartridgeArtwork(scene);
    const mask = canvasTexture(256, 256);
    mask.context.fillStyle = '#000';
    mask.context.fillRect(0, 0, 256, 256);
    mask.context.fillStyle = '#fff';
    mask.context.beginPath();
    mask.context.roundRect(0, 0, 256, 256, 20);
    mask.context.fill();
    const face = new THREE.Mesh(
        new THREE.PlaneGeometry(0.44, 0.475),
        new THREE.MeshStandardMaterial({
            map: artwork,
            alphaMap: mask.texture,
            alphaTest: 0.5,
            roughness: 0.6,
            metalness: 0,
        }),
    );
    face.position.set(0, -0.003, 0.057);
    group.add(face);
    const back = lettering('GOLDENEYE 007', 0.55, 0.07, '#5f6660');
    back.rotation.y = Math.PI;
    back.position.set(0, 0.08, -0.078);
    group.add(back);
    group.name = 'cartridge';
    scene.add(group);
    return group;
}
