import * as THREE from 'three';
import { canvasTexture } from './geometry';

// Deterministic surface detail generated locally; no downloaded texture packs.
export function texturedPlastic(color: number, roughness = 0.55) {
    const grain = canvasTexture(128, 128);
    const image = grain.context.createImageData(128, 128);
    for (let index = 0; index < image.data.length; index += 4) {
        const value = 112 + ((index * 73 + Math.floor(index / 512) * 37) % 41);
        image.data.set([value, value, value, 255], index);
    }
    grain.context.putImageData(image, 0, 0);
    grain.texture.colorSpace = THREE.NoColorSpace;
    grain.texture.wrapS = grain.texture.wrapT = THREE.RepeatWrapping;
    grain.texture.repeat.set(5, 5);
    return new THREE.MeshStandardMaterial({
        color,
        roughness,
        metalness: 0.02,
        bumpMap: grain.texture,
        bumpScale: 0.002,
    });
}

export function woodMaterial() {
    const grain = canvasTexture(256, 256);
    const ctx = grain.context;
    ctx.fillStyle = '#63503b';
    ctx.fillRect(0, 0, 256, 256);
    for (let index = 0; index < 600; index++) {
        const x = (index * 13.79) % 256;
        ctx.strokeStyle = index % 3 ? '#251d1618' : '#af91651b';
        ctx.lineWidth = index % 5 ? 0.6 : 1.5;
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.bezierCurveTo(x + Math.sin(index) * 7, 80, x - Math.cos(index) * 9, 170, x + 2, 256);
        ctx.stroke();
    }
    grain.texture.wrapS = grain.texture.wrapT = THREE.RepeatWrapping;
    return new THREE.MeshStandardMaterial({
        map: grain.texture,
        roughness: 0.65,
        bumpMap: grain.texture,
        bumpScale: 0.004,
    });
}

export function lettering(text: string, width: number, height: number, color = '#b6b6aa') {
    const label = canvasTexture(512, 96);
    label.context.clearRect(0, 0, 512, 96);
    label.context.fillStyle = color;
    label.context.font = 'bold 48px monospace';
    label.context.textAlign = 'center';
    label.context.fillText(text, 256, 65);
    return new THREE.Mesh(
        new THREE.PlaneGeometry(width, height),
        new THREE.MeshBasicMaterial({ map: label.texture, transparent: true, depthWrite: false }),
    );
}

export function extrudedProfile(
    shape: THREE.Shape,
    depth: number,
    material: THREE.Material,
    bevel = 0.025,
) {
    const geometry = new THREE.ExtrudeGeometry(shape, {
        depth,
        bevelEnabled: true,
        bevelSegments: 3,
        steps: 1,
        bevelSize: bevel,
        bevelThickness: bevel,
        curveSegments: 12,
    });
    const mesh = new THREE.Mesh(geometry, material);
    mesh.castShadow = mesh.receiveShadow = true;
    return mesh;
}
