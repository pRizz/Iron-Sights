import * as THREE from 'three';
import { box, cable } from './geometry';
import { createControllerPlug } from './controller-plug';

// Swayne's attributed model replaces the procedural shell. See MODELS.md.
export function createConsole(scene: THREE.Scene, model: THREE.Group) {
    const group = model;
    group.scale.setScalar(1.6 / 0.2657907009124756);
    group.position.set(-0.2, 0.04, 1.22);
    scene.add(group);
    const power = group.getObjectByName('PowerSwitch');
    if (!power) throw new Error('The console model is missing its power switch.');
    const homeZ = power.position.z;
    const ledMaterial = new THREE.MeshStandardMaterial({
        color: 0x520505,
        emissive: 0xff0000,
        emissiveIntensity: 0,
        roughness: 0.2,
        toneMapped: false,
    });
    const led = box(group, [0.004, 0.003, 0.002], [0, 0.027, 0.0963], ledMaterial, 0.0006);
    led.name = 'PowerLED';
    const controllerLead = createControllerPlug(group);
    const powerHit = box(
        group,
        [0.065, 0.025, 0.055],
        power.position.toArray(),
        new THREE.MeshBasicMaterial({ visible: false }),
    );
    for (const points of [
        [
            [-0.6, 0.18, 0.6],
            [-1.1, 0.055, 0.3],
            [-1.4, 0.035, -0.5],
            [-0.8, 0.65, -2],
        ],
        [
            [0.2, 0.17, 0.6],
            [0.7, 0.035, 0.3],
            [1.6, 0.03, 0.8],
            [2.3, 0.03, -2.8],
        ],
        [
            controllerLead.exit,
            controllerLead.lead,
            [-0.85, 0.065, 2.35],
            [-1.5, 0.035, 2.4],
            [-1.2, 0.09, 2.4],
        ],
    ])
        cable(scene, points);
    return {
        group,
        power,
        powerHit,
        ledMaterial,
        setPower(progress: number) {
            power.position.z = homeZ - progress * 0.012;
        },
    };
}
