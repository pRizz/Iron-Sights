import * as THREE from 'three';

// Marshle's controller retains its source mesh, normals, UVs and base-color image.
export function createController(scene: THREE.Scene, model: THREE.Group) {
    const bounds = new THREE.Box3().setFromObject(model);
    const center = bounds.getCenter(new THREE.Vector3());
    const scale = 0.88 / (bounds.max.x - bounds.min.x);
    model.position.set(-center.x * scale, -bounds.min.y * scale, -center.z * scale);
    model.scale.setScalar(scale);
    const group = new THREE.Group();
    group.position.set(-1.2, 0.035, 2.6);
    group.rotation.y = -0.18;
    group.add(model);
    scene.add(group);
    return group;
}
