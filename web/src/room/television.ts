import * as THREE from 'three';
import { box, canvasTexture, plastic } from './geometry';
import { lettering, texturedPlastic, woodMaterial } from './materials';
import vertexShader from './crt.vert.glsl?raw';
import fragmentShader from './crt.frag.glsl?raw';
import { TV_SCREEN } from './framing';

export function createTelevision(scene: THREE.Scene) {
    const tv = new THREE.Group();
    tv.position.set(0, 2.05, -1.65);
    scene.add(tv);
    const body = texturedPlastic(0x42413b, 0.56);
    const bezel = plastic(0x151918, 0.6);
    box(tv, [2.91, 2.13, 1.2], [0, 0, -0.04], body, 0.14);
    box(tv, [2.77, 2.01, 0.13], [0, 0.01, 0.57], bezel, 0.06);
    box(tv, [2.61, 1.88, 0.08], [-0.06, 0.05, 0.65], plastic(0x252923), 0.03);
    const screen = canvasTexture(640, 480);
    const geometry = new THREE.PlaneGeometry(TV_SCREEN.width, TV_SCREEN.height, 40, 32);
    const positions = geometry.attributes.position;
    for (let index = 0; index < positions.count; index++) {
        const x = positions.getX(index) / (TV_SCREEN.width / 2);
        const y = positions.getY(index) / (TV_SCREEN.height / 2);
        positions.setZ(index, 0.13 * (1 - x * x) * (1 - y * y));
    }
    geometry.computeVertexNormals();
    const signal = new THREE.ShaderMaterial({
        vertexShader,
        fragmentShader,
        uniforms: {
            screen: { value: screen.texture },
            time: { value: 0 },
            motion: { value: 1 },
            power: { value: 0 },
        },
    });
    const surface = new THREE.Mesh(geometry, signal);
    surface.position.set(-0.06, 0.05, 0.7);
    tv.add(surface);
    // A faint physical reflection over the phosphor surface gives the glass depth.
    const glass = new THREE.Mesh(
        geometry.clone(),
        new THREE.MeshPhysicalMaterial({
            color: 0x8599aa,
            transparent: true,
            opacity: 0.055,
            roughness: 0.09,
            metalness: 0.12,
            clearcoat: 1,
            clearcoatRoughness: 0.12,
            depthWrite: false,
        }),
    );
    glass.position.copy(surface.position).add(new THREE.Vector3(0, 0, 0.003));
    tv.add(glass);
    for (let index = 0; index < 15; index++)
        box(tv, [0.028, 0.65, 0.012], [1.24 + (index % 3) * 0.045, 0.25, 0.61], bezel, 0.002);
    const knob = new THREE.Mesh(
        new THREE.CylinderGeometry(0.06, 0.06, 0.045, 24),
        plastic(0x7b7d72),
    );
    knob.rotation.x = Math.PI / 2;
    knob.position.set(1.22, -0.83, 0.72);
    knob.name = 'controls';
    tv.add(knob);
    for (let index = 0; index < 4; index++)
        box(tv, [0.07, 0.035, 0.03], [0.77 + index * 0.095, -0.83, 0.7], plastic(0x51564d), 0.008);
    const name = lettering('MIDAS', 0.22, 0.035, '#93958a');
    name.position.set(-0.06, -0.945, 0.65);
    tv.add(name);
    const help = lettering('CONTROLS', 0.18, 0.025, '#a7a89b');
    help.position.set(1.22, -0.95, 0.7);
    tv.add(help);
    const wood = woodMaterial();
    for (const y of [0.14, 0.85]) box(scene, [3.4, 0.14, 1.5], [0, y, -1.65], wood, 0.035);
    for (const x of [-1.53, 1.53]) box(scene, [0.16, 0.8, 1.4], [x, 0.47, -1.65], wood);
    box(scene, [3.03, 0.66, 0.06], [0, 0.49, -2.3], wood);
    box(scene, [1.6, 0.13, 0.75], [0.3, 0.28, -1.6], plastic(0x252726), 0.03);
    const glow = new THREE.PointLight(0x4979ff, 8, 6, 2);
    glow.position.set(0, 1.8, -0.4);
    scene.add(glow);
    return { ...screen, glow, knob, signal };
}
