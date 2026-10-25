import * as THREE from 'three';
import { box, cable, plastic } from './geometry';
import { extrudedProfile, lettering, texturedPlastic } from './materials';

function shellProfile() {
    const shape = new THREE.Shape();
    shape.moveTo(-0.7, -0.45);
    shape.bezierCurveTo(-0.82, -0.42, -0.83, -0.24, -0.79, 0.22);
    shape.quadraticCurveTo(-0.73, 0.59, -0.53, 0.59);
    shape.quadraticCurveTo(0, 0.47, 0.53, 0.59);
    shape.quadraticCurveTo(0.73, 0.59, 0.79, 0.22);
    shape.bezierCurveTo(0.83, -0.24, 0.82, -0.42, 0.7, -0.45);
    shape.quadraticCurveTo(0, -0.54, -0.7, -0.45);
    return shape;
}

function shell(parent: THREE.Group, material: THREE.Material, height: number, y: number) {
    const mesh = extrudedProfile(shellProfile(), height, material, 0.025);
    mesh.rotation.x = -Math.PI / 2;
    mesh.position.y = y;
    parent.add(mesh);
}

export function createConsole(scene: THREE.Scene) {
    const group = new THREE.Group();
    group.position.set(-0.2, 0.12, 1.1);
    scene.add(group);
    const dark = texturedPlastic(0x414345, 0.56);
    const seam = plastic(0x101316, 0.8);
    shell(group, seam, 0.11, 0.015);
    shell(group, dark, 0.115, 0.14);
    // The top shoulder slopes into the central cartridge opening.
    box(group, [1.08, 0.13, 0.61], [0, 0.29, -0.15], dark, 0.09);
    const slot = box(group, [0.97, 0.025, 0.19], [0, 0.369, -0.12], seam, 0.02);
    for (const z of [-0.178, -0.061]) box(group, [0.9, 0.018, 0.055], [0, 0.377, z], dark, 0.008);
    for (const [index, x] of [-0.53, -0.18, 0.18, 0.53].entries()) {
        box(group, [0.255, 0.15, 0.045], [x, 0.14, 0.533], seam, 0.025);
        box(group, [0.195, 0.085, 0.025], [x, 0.14, 0.56], plastic(0x686b6c), 0.018);
        box(group, [0.15, 0.055, 0.03], [x, 0.14, 0.577], seam, 0.012);
        for (let pin = 0; pin < 3; pin++)
            box(
                group,
                [0.009, 0.012, 0.004],
                [x - 0.036 + pin * 0.036, 0.14, 0.594],
                plastic(0xb4a574),
                0.002,
            );
        const number = lettering(String(index + 1), 0.06, 0.015, '#b0b0a5');
        number.position.set(x, 0.235, 0.562);
        group.add(number);
    }
    for (let index = 0; index < 11; index++) {
        for (const x of [-0.64, 0.64])
            box(group, [0.14, 0.012, 0.018], [x, 0.285, -0.4 + index * 0.054], seam, 0.003);
    }
    box(group, [0.43, 0.014, 0.27], [0, 0.275, 0.31], seam, 0.02);
    box(group, [0.39, 0.016, 0.23], [0, 0.286, 0.31], dark, 0.018);
    const pak = lettering('MEMORY EXPANSION', 0.3, 0.035, '#6e7170');
    pak.rotation.x = -Math.PI / 2;
    pak.position.set(0, 0.296, 0.34);
    group.add(pak);
    box(group, [0.235, 0.02, 0.23], [-0.57, 0.28, 0.28], seam, 0.015);
    const power = box(
        group,
        [0.18, 0.043, 0.15],
        [-0.57, 0.305, 0.28],
        texturedPlastic(0x842b25),
        0.016,
    );
    power.name = 'power';
    const powerText = lettering('POWER', 0.18, 0.026);
    powerText.rotation.x = -Math.PI / 2;
    powerText.position.set(-0.57, 0.283, 0.45);
    group.add(powerText);
    box(group, [0.19, 0.019, 0.17], [0.57, 0.279, 0.29], seam);
    box(group, [0.14, 0.025, 0.12], [0.57, 0.294, 0.29], dark, 0.012);
    const ledMaterial = new THREE.MeshStandardMaterial({
        color: 0x520c05,
        emissive: 0xff1c06,
        emissiveIntensity: 0,
        roughness: 0.2,
    });
    box(group, [0.045, 0.025, 0.023], [-0.57, 0.24, 0.55], ledMaterial, 0.009);
    const badge = lettering('NINTENDO 64', 0.3, 0.045, '#b5b5ac');
    badge.rotation.x = -Math.PI / 2;
    badge.position.set(0, 0.285, 0.48);
    group.add(badge);
    for (const x of [-0.57, 0.57])
        for (const z of [-0.35, 0.38]) box(group, [0.17, 0.08, 0.17], [x, -0.075, z], seam);
    cable(scene, [
        [-0.6, 0.18, 0.6],
        [-1.1, 0.055, 0.3],
        [-1.4, 0.035, -0.5],
        [-0.8, 0.65, -2],
    ]);
    cable(scene, [
        [0.2, 0.17, 0.6],
        [0.7, 0.035, 0.3],
        [1.6, 0.03, 0.8],
        [2.3, 0.03, -2.8],
    ]);
    cable(scene, [
        [-0.73, 0.26, 1.69],
        [-0.85, 0.065, 2.1],
        [-1.5, 0.035, 2.4],
        [-1.2, 0.09, 2.4],
    ]);
    return { group, power, ledMaterial, slot };
}
