import * as THREE from 'three';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

// Batch only opaque static props. Interactive, emissive and glass parts stay independent.
export function batchStaticProps(scene: THREE.Scene, moving: THREE.Object3D[]) {
    scene.updateMatrixWorld(true);
    const groups = new Map<
        string,
        THREE.Mesh<THREE.BufferGeometry, THREE.MeshStandardMaterial>[]
    >();
    scene.traverse((object) => {
        if (!(object instanceof THREE.Mesh)) return;
        const material = object.material;
        if (
            !(material instanceof THREE.MeshStandardMaterial) ||
            material.transparent ||
            material.emissive.getHex()
        )
            return;
        let maybeAncestor: THREE.Object3D | null = object;
        while (maybeAncestor) {
            if (moving.includes(maybeAncestor)) return;
            maybeAncestor = maybeAncestor.parent;
        }
        const key = [
            material.color.getHex(),
            material.roughness,
            material.metalness,
            material.map?.uuid,
            material.bumpMap?.uuid,
            material.bumpScale,
            material.side,
        ].join(':');
        const group = groups.get(key) || [];
        group.push(object as THREE.Mesh<THREE.BufferGeometry, THREE.MeshStandardMaterial>);
        groups.set(key, group);
    });
    for (const meshes of groups.values()) {
        if (meshes.length < 2) continue;
        const transformed = meshes.map((mesh) => {
            const geometry = mesh.geometry.index
                ? mesh.geometry.toNonIndexed()
                : mesh.geometry.clone();
            return geometry.applyMatrix4(mesh.matrixWorld);
        });
        const geometry = mergeGeometries(transformed);
        transformed.forEach((item) => item.dispose());
        if (!geometry) continue;
        const batch = new THREE.Mesh(geometry, meshes[0].material);
        batch.castShadow = meshes.some((mesh) => mesh.castShadow);
        batch.receiveShadow = true;
        for (const mesh of meshes) {
            mesh.removeFromParent();
            mesh.geometry.dispose();
        }
        scene.add(batch);
    }
}
