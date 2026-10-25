import * as THREE from 'three';
import { RoundedBoxGeometry } from 'three/addons/geometries/RoundedBoxGeometry.js';

export function plastic(color: number, roughness = 0.55) {
    return new THREE.MeshStandardMaterial({ color, roughness, metalness: 0.06 });
}

export function box(
    parent: THREE.Object3D,
    dimensions: [number, number, number],
    position: [number, number, number],
    material: THREE.Material,
    radius = 0.03,
) {
    const segments = Math.min(...dimensions) > 0.1 ? 2 : 1;
    const mesh = new THREE.Mesh(new RoundedBoxGeometry(...dimensions, segments, radius), material);
    mesh.position.set(...position);
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    parent.add(mesh);
    return mesh;
}

export function cable(parent: THREE.Object3D, points: number[][], radius = 0.018) {
    const curve = new THREE.CatmullRomCurve3(points.map(([x, y, z]) => new THREE.Vector3(x, y, z)));
    const mesh = new THREE.Mesh(
        new THREE.TubeGeometry(curve, 64, radius, 8, false),
        plastic(0x111213),
    );
    mesh.castShadow = true;
    parent.add(mesh);
}

export function canvasTexture(width: number, height: number) {
    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = height;
    const context = canvas.getContext('2d');
    if (!context) throw new Error('The room could not create its screen textures.');
    const texture = new THREE.CanvasTexture(canvas);
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.anisotropy = 4;
    return { canvas, context, texture };
}

export function disposeScene(scene: THREE.Scene): void {
    const geometries = new Set<THREE.BufferGeometry>();
    const materials = new Set<THREE.Material>();
    const textures = new Set<THREE.Texture>();
    scene.traverse((object) => {
        if (!(object instanceof THREE.Mesh) && !(object instanceof THREE.Sprite)) return;
        geometries.add(object.geometry);
        for (const material of Array.isArray(object.material)
            ? object.material
            : [object.material]) {
            materials.add(material);
            for (const value of Object.values(material))
                if (value instanceof THREE.Texture) textures.add(value);
            if (material instanceof THREE.ShaderMaterial)
                for (const uniform of Object.values(material.uniforms))
                    if (uniform.value instanceof THREE.Texture) textures.add(uniform.value);
        }
    });
    for (const geometry of geometries) geometry.dispose();
    for (const material of materials) material.dispose();
    for (const texture of textures) texture.dispose();
    scene.traverse((object) => {
        if (object instanceof THREE.Light) object.dispose();
    });
}
