import * as THREE from 'three';
import { box, plastic } from './geometry';
import { extrudedProfile, texturedPlastic } from './materials';

export function createController(scene: THREE.Scene) {
    const group = new THREE.Group();
    group.position.set(-1.2, 0.08, 2.6);
    group.rotation.y = -0.18;
    const outline = new THREE.Shape();
    outline.moveTo(-0.31, 0.18);
    outline.bezierCurveTo(-0.48, 0.1, -0.42, -0.15, -0.34, -0.37);
    outline.quadraticCurveTo(-0.25, -0.48, -0.19, -0.34);
    outline.lineTo(-0.12, -0.12);
    outline.lineTo(-0.08, -0.43);
    outline.quadraticCurveTo(0, -0.55, 0.08, -0.43);
    outline.lineTo(0.12, -0.12);
    outline.lineTo(0.19, -0.34);
    outline.quadraticCurveTo(0.25, -0.48, 0.34, -0.37);
    outline.bezierCurveTo(0.42, -0.15, 0.48, 0.1, 0.31, 0.18);
    outline.quadraticCurveTo(0, 0.3, -0.31, 0.18);
    const body = extrudedProfile(outline, 0.065, texturedPlastic(0x868c88, 0.7), 0.025);
    body.rotation.x = -Math.PI / 2;
    group.add(body);
    const dark = plastic(0x272c2a);
    box(group, [0.15, 0.035, 0.045], [-0.25, 0.1, -0.01], dark, 0.006);
    box(group, [0.045, 0.035, 0.15], [-0.25, 0.1, -0.01], dark, 0.006);
    const button = (x: number, z: number, color: number, radius = 0.035, height = 0.035) => {
        const mesh = new THREE.Mesh(
            new THREE.CylinderGeometry(radius, radius, height, 16),
            plastic(color),
        );
        mesh.position.set(x, 0.1 + height / 2, z);
        group.add(mesh);
    };
    button(0, 0.04, 0x373e38, 0.065, 0.018);
    button(0, 0.04, 0xa8aaa1, 0.028, 0.09);
    button(0.19, 0.075, 0x126f42, 0.045);
    button(0.12, 0.005, 0x244987, 0.035);
    button(0, -0.13, 0xa4312b, 0.023);
    for (const [x, z] of [
        [0.25, -0.09],
        [0.32, -0.03],
        [0.25, 0.03],
        [0.18, -0.09],
    ])
        button(x, z, 0xb49b32, 0.025);
    scene.add(group);
}
