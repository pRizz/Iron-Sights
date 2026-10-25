import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { disposeScene } from './geometry';

export async function loadRoomModels() {
    const loader = new GLTFLoader();
    const loaded = await Promise.allSettled(
        ['console', 'controller'].map(async (name) => {
            const gltf = await loader.loadAsync(import.meta.env.BASE_URL + `room/${name}.glb`);
            gltf.scene.traverse((object) => {
                if (!(object instanceof THREE.Mesh)) return;
                object.castShadow = true;
                object.receiveShadow = true;
            });
            return gltf.scene;
        }),
    );
    if (loaded.some((result) => result.status === 'rejected')) {
        for (const result of loaded) {
            if (result.status !== 'fulfilled') continue;
            const scene = new THREE.Scene();
            scene.add(result.value);
            disposeScene(scene);
        }
        throw new Error('The licensed room models could not be loaded.');
    }
    const [consoleResult, controllerResult] = loaded;
    if (consoleResult.status !== 'fulfilled' || controllerResult.status !== 'fulfilled')
        throw new Error('The room models are incomplete.');
    return { console: consoleResult.value, controller: controllerResult.value };
}
