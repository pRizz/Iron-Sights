// Original procedural geometry; cartridge artwork is the user-supplied photo.
import * as THREE from 'three';
import { box, cable, canvasTexture, plastic } from './geometry';

export function createConsole(scene: THREE.Scene) {
    const group = new THREE.Group();
    group.position.set(-0.2, 0.12, 1.1);
    scene.add(group);
    const dark = plastic(0x333536, 0.45);
    const seam = plastic(0x141619);
    box(group, [1.55, 0.19, 1.14], [0, 0.08, 0], seam, 0.08);
    box(group, [1.58, 0.19, 1.12], [0, 0.2, 0], dark, 0.1);
    box(group, [1.1, 0.1, 0.55], [0, 0.3, -0.15], dark, 0.1);
    box(group, [0.8, 0.026, 0.18], [0, 0.36, -0.12], seam, 0.025);
    for (const x of [-0.53, -0.18, 0.18, 0.53]) {
        box(group, [0.22, 0.13, 0.025], [x, 0.16, 0.558], seam, 0.035);
        box(group, [0.16, 0.07, 0.03], [x, 0.16, 0.573], plastic(0x777978), 0.02);
        box(group, [0.12, 0.045, 0.035], [x, 0.16, 0.592], seam, 0.012);
    }
    for (let index = 0; index < 9; index++) {
        for (const x of [-0.61, 0.61])
            box(group, [0.14, 0.014, 0.022], [x, 0.299, -0.32 + index * 0.065], seam, 0.004);
    }
    box(group, [0.38, 0.02, 0.31], [0, 0.31, 0.32], seam, 0.025);
    const power = box(group, [0.19, 0.055, 0.17], [-0.57, 0.32, 0.28], plastic(0x952b25), 0.02);
    power.name = 'power';
    const ledMaterial = new THREE.MeshStandardMaterial({
        color: 0x4b0803,
        emissive: 0xff1605,
        emissiveIntensity: 0,
        roughness: 0.2,
    });
    box(group, [0.045, 0.025, 0.025], [-0.57, 0.24, 0.557], ledMaterial, 0.009);
    box(group, [0.15, 0.03, 0.13], [0.57, 0.31, 0.29], dark, 0.015);
    cable(scene, [
        [-0.6, 0.18, 0.6],
        [-1.1, 0.06, 0.3],
        [-1.4, 0.045, -0.5],
        [-0.8, 0.5, -2],
    ]);
    cable(scene, [
        [0.2, 0.17, 0.6],
        [0.7, 0.03, 0.3],
        [1.6, 0.025, 0.8],
        [2.3, 0.03, -2.8],
    ]);
    cable(scene, [
        [-0.73, 0.28, 1.7],
        [-0.85, 0.07, 2.1],
        [-1.5, 0.03, 2.4],
        [-1.8, 0.03, 2.8],
    ]);
    return { group, power, ledMaterial };
}

export function createCartridge(scene: THREE.Scene) {
    const group = new THREE.Group();
    const shell = plastic(0x9b9e9b, 0.42);
    box(group, [0.88, 0.56, 0.15], [0, 0, 0], shell, 0.065);
    box(group, [0.79, 0.045, 0.17], [0, -0.19, 0], shell, 0.012);
    box(group, [0.72, 0.025, 0.155], [0, -0.27, 0], plastic(0x454746), 0.005);
    for (const x of [-0.37, 0.37])
        for (let index = 0; index < 5; index++)
            box(group, [0.04, 0.012, 0.01], [x, -0.05 + index * 0.035, 0.078], shell, 0.003);
    const artwork = new THREE.TextureLoader().load(
        import.meta.env.BASE_URL + 'room/cartridge-art.avif',
    );
    artwork.colorSpace = THREE.SRGBColorSpace;
    artwork.anisotropy = 4;
    // Sample only the printed label in the unchanged 1000x1000 product photo.
    // UV origin is bottom-left: x 272..742, y 270..795 in image coordinates.
    artwork.offset.set(0.272, 0.205);
    artwork.repeat.set(0.47, 0.525);
    const mask = canvasTexture(256, 256);
    mask.context.fillStyle = '#000';
    mask.context.fillRect(0, 0, 256, 256);
    mask.context.fillStyle = '#fff';
    mask.context.beginPath();
    mask.context.roundRect(0, 0, 256, 256, 20);
    mask.context.fill();
    const face = new THREE.Mesh(
        new THREE.PlaneGeometry(0.44, 0.49),
        new THREE.MeshStandardMaterial({
            map: artwork,
            alphaMap: mask.texture,
            alphaTest: 0.5,
            roughness: 0.5,
            metalness: 0.05,
        }),
    );
    face.position.set(0, 0.005, 0.078);
    group.add(face);
    group.name = 'cartridge';
    scene.add(group);
    return group;
}

export function createTelevision(scene: THREE.Scene) {
    const tv = new THREE.Group();
    tv.position.set(0, 2.05, -1.65);
    scene.add(tv);
    const body = plastic(0x313230, 0.42);
    box(tv, [2.9, 2.15, 1.22], [0, 0, 0], body, 0.18);
    box(tv, [2.65, 1.9, 0.09], [0, 0.02, 0.64], plastic(0x101314), 0.18);
    const screen = canvasTexture(640, 480);
    const geometry = new THREE.PlaneGeometry(2.35, 1.7, 32, 24);
    const positions = geometry.attributes.position;
    for (let index = 0; index < positions.count; index++) {
        const x = positions.getX(index) / 1.175;
        const y = positions.getY(index) / 0.85;
        positions.setZ(index, 0.1 * (1 - x * x) * (1 - y * y));
    }
    geometry.computeVertexNormals();
    const surface = new THREE.Mesh(geometry, new THREE.MeshBasicMaterial({ map: screen.texture }));
    surface.position.set(-0.06, 0.05, 0.7);
    tv.add(surface);
    const knob = new THREE.Mesh(
        new THREE.CylinderGeometry(0.06, 0.06, 0.045, 20),
        plastic(0x8a8c83),
    );
    knob.rotation.x = Math.PI / 2;
    knob.position.set(1.22, -0.83, 0.72);
    knob.name = 'controls';
    tv.add(knob);
    for (let index = 0; index < 6; index++)
        box(tv, [0.11, 0.012, 0.02], [0.8, -0.7 + index * 0.045, 0.69], plastic(0x101110), 0.004);
    const wood = plastic(0x33251d, 0.48);
    for (const y of [0.14, 0.85]) box(scene, [3.4, 0.14, 1.5], [0, y, -1.65], wood, 0.035);
    for (const x of [-1.53, 1.53]) box(scene, [0.16, 0.8, 1.4], [x, 0.47, -1.65], wood);
    box(scene, [1.6, 0.13, 0.75], [0.3, 0.28, -1.6], plastic(0x15171a), 0.03);
    const glow = new THREE.PointLight(0x255dff, 19, 6, 2);
    glow.position.set(0, 1.8, -0.5);
    scene.add(glow);
    return { ...screen, glow, knob };
}
