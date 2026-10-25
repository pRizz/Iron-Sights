import * as THREE from 'three';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
import { box, plastic } from './geometry';
import { woodMaterial } from './materials';

export function furnishRoom(scene: THREE.Scene, renderer: THREE.WebGLRenderer) {
    const wood = woodMaterial();
    for (let index = -12; index <= 12; index++)
        box(scene, [0.48, 0.06, 14], [index * 0.49, -0.035, 0], wood, 0.006);
    box(scene, [14, 7, 0.2], [0, 3.4, -3.1], plastic(0x242c29, 0.96));
    box(scene, [14, 0.12, 0.12], [0, 0.08, -2.95], plastic(0x4c473d));
    for (const x of [-4.5, -3, 3, 4.5])
        box(scene, [0.022, 5, 0.02], [x, 2.5, -2.97], plastic(0x2b302d));
    const ambient = new THREE.HemisphereLight(0xa8b9ca, 0x493021, 0.65);
    scene.add(ambient);
    const fill = new THREE.SpotLight(0xffd8ac, 26, 14, Math.PI / 4, 0.85, 2);
    fill.position.set(3.5, 5.5, 4.5);
    fill.target.position.set(0, 0.5, 1);
    fill.castShadow = true;
    fill.shadow.mapSize.set(1024, 1024);
    fill.shadow.bias = -0.001;
    scene.add(fill, fill.target);
    const rim = new THREE.PointLight(0x92aaca, 9, 8, 2);
    rim.position.set(-3.5, 2.7, 1);
    scene.add(rim);
    const environment = new RoomEnvironment();
    const generator = new THREE.PMREMGenerator(renderer);
    const target = generator.fromScene(environment, 0.04, 0.1, 20, { size: 64 });
    scene.environment = target.texture;
    scene.environmentIntensity = 0.085;
    environment.dispose();
    generator.dispose();
    return () => target.dispose();
}
