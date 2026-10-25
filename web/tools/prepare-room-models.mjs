// Rebuild the attributed room models from pinned, hash-checked public sources.
// Run from web/: node tools/prepare-room-models.mjs. Downloads/logs stay in build/.
import { mkdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { GLTFExporter } from 'three/addons/exporters/GLTFExporter.js';
import { partitionGeometry } from './model-geometry.mjs';

const output = new URL('../public/room/', import.meta.url);
const audit = new URL('../../build/web-room/model-preparation/', import.meta.url);
await mkdir(audit, { recursive: true });
await mkdir(output, { recursive: true });
const sources = [
    [
        'console.glb',
        'https://raw.githubusercontent.com/k0bolde/tfjam23/7cd0cc7f65917a56fa4232a71f2d005bb5f911be/assets/n64.glb',
        '7461092b58cfa3e91214dcd5dbdf04a34532a0e76275c93572cbd82124265a12',
    ],
    [
        'controller.bin',
        'https://raw.githubusercontent.com/maxnau89/mupen64plus-ae-quest/e77a89c3388a27ae29c464ad2f75e3985dffc869/app/src/main/assets/quest/n64_controller.bin',
        'aeed171ad33c1366baf466cc1dcc823628307cc0729166dd712b5d00db3d1716',
    ],
    [
        'controller.png',
        'https://raw.githubusercontent.com/maxnau89/mupen64plus-ae-quest/e77a89c3388a27ae29c464ad2f75e3985dffc869/app/src/main/assets/quest/n64_controller.png',
        'b36feec550afc2f3dffe857022d0ccfb0eece8d0bef2c306eae7a48feeaee99c',
    ],
];
const digest = (bytes) => createHash('sha256').update(bytes).digest('hex');
const inputs = [];
for (const [name, url, sha256] of sources) {
    console.log('Fetching pinned source:', name);
    const response = await fetch(url);
    assert.equal(response.status, 200);
    const bytes = Buffer.from(await response.arrayBuffer());
    assert.equal(digest(bytes), sha256, 'Source digest mismatch: ' + name);
    await writeFile(new URL(name, audit), bytes);
    inputs.push(bytes);
}

// The exporter only needs this browser adapter for its binary Blob output.
globalThis.FileReader = class {
    readAsArrayBuffer(blob) {
        void blob.arrayBuffer().then((result) => {
            this.result = result;
            this.onloadend?.();
        });
    }
};

async function prepareConsole(bytes) {
    const gltf = await new GLTFLoader().parseAsync(
        bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength),
        '',
    );
    const model = gltf.scene.getObjectByName('Console');
    assert.ok(model, 'Pinned console node missing');
    model.traverse((object) => {
        if (!object.isMesh) return;
        const original = object.material;
        object.material = new THREE.MeshStandardMaterial({
            name: original.name,
            color: original.name === 'DarkPlastic.001' ? 0x333333 : original.color,
            roughness: 0.65,
            metalness: 0.02,
        });
    });
    const shell = model.getObjectByName('Cube018');
    assert.ok(shell?.isMesh);
    // Isolate the existing 72-triangle slide switch without changing its shape.
    const [staticGeometry, powerGeometry] = partitionGeometry(
        shell.geometry,
        (point) =>
            point.x > -0.08 &&
            point.x < -0.05 &&
            point.y > 0.053 &&
            point.y < 0.066 &&
            point.z > 0.036 &&
            point.z < 0.073,
    );
    assert.equal(powerGeometry.attributes.position.count, 72 * 3);
    shell.geometry.dispose();
    shell.geometry = staticGeometry;
    powerGeometry.computeBoundingBox();
    const center = powerGeometry.boundingBox.getCenter(new THREE.Vector3());
    powerGeometry.translate(-center.x, -center.y, -center.z);
    const power = new THREE.Mesh(powerGeometry, shell.material.clone());
    power.material.color.setHex(0x842b25);
    power.name = 'PowerSwitch';
    power.position.copy(center);
    model.add(power);
    const scene = new THREE.Scene();
    scene.add(model);
    scene.userData = {
        source: 'Simple N64 model by Swayne',
        license: 'CC BY 4.0',
        changes: 'Console only; isolated and recolored power switch.',
    };
    return Buffer.from(await new GLTFExporter().parseAsync(scene, { binary: true }));
}

function encodeGLB(json, binary) {
    const document = Buffer.from(JSON.stringify(json));
    const jsonLength = Math.ceil(document.length / 4) * 4;
    const binLength = Math.ceil(binary.length / 4) * 4;
    const result = Buffer.alloc(28 + jsonLength + binLength);
    result.writeUInt32LE(0x46546c67, 0);
    result.writeUInt32LE(2, 4);
    result.writeUInt32LE(result.length, 8);
    result.writeUInt32LE(jsonLength, 12);
    result.writeUInt32LE(0x4e4f534a, 16);
    result.fill(32, 20, 20 + jsonLength);
    document.copy(result, 20);
    result.writeUInt32LE(binLength, 20 + jsonLength);
    result.writeUInt32LE(0x004e4942, 24 + jsonLength);
    binary.copy(result, 28 + jsonLength);
    return result;
}

function prepareController(bytes, texture) {
    assert.equal(bytes.toString('ascii', 0, 4), 'N64M');
    assert.equal(bytes.readUInt32LE(4), 1);
    const count = bytes.readUInt32LE(8);
    assert.equal(count, 29844);
    assert.equal(bytes.length, 12 + count * 36);
    const geometry = bytes.subarray(12);
    const min = [Infinity, Infinity, Infinity];
    const max = [-Infinity, -Infinity, -Infinity];
    for (let vertex = 0; vertex < count; vertex++)
        for (let axis = 0; axis < 3; axis++) {
            const value = geometry.readFloatLE(vertex * 36 + axis * 4);
            min[axis] = Math.min(min[axis], value);
            max[axis] = Math.max(max[axis], value);
        }
    const json = {
        asset: {
            version: '2.0',
            generator: 'Project Midas room model preparation',
            copyright:
                'N64 Controller by Marshle (formerly CodySmith1), CC BY 4.0; compact mesh/texture redistribution by maxnau89.',
        },
        scene: 0,
        scenes: [{ nodes: [0] }],
        nodes: [{ name: 'Controller', mesh: 0 }],
        meshes: [
            {
                primitives: [
                    { attributes: { POSITION: 0, NORMAL: 1, TEXCOORD_0: 2 }, material: 0 },
                ],
            },
        ],
        buffers: [{ byteLength: geometry.length + texture.length }],
        bufferViews: [
            {
                buffer: 0,
                byteOffset: 0,
                byteLength: geometry.length,
                byteStride: 36,
                target: 34962,
            },
            { buffer: 0, byteOffset: geometry.length, byteLength: texture.length },
        ],
        accessors: [
            { bufferView: 0, byteOffset: 0, componentType: 5126, count, type: 'VEC3', min, max },
            { bufferView: 0, byteOffset: 12, componentType: 5126, count, type: 'VEC3' },
            { bufferView: 0, byteOffset: 24, componentType: 5126, count, type: 'VEC2' },
        ],
        materials: [
            {
                name: 'Controller plastic',
                pbrMetallicRoughness: {
                    baseColorTexture: { index: 0 },
                    metallicFactor: 0.02,
                    roughnessFactor: 0.65,
                },
            },
        ],
        textures: [{ source: 0 }],
        images: [{ bufferView: 1, mimeType: 'image/png' }],
    };
    return encodeGLB(json, Buffer.concat([geometry, texture]));
}

const models = [
    ['console.glb', await prepareConsole(inputs[0])],
    ['controller.glb', prepareController(inputs[1], inputs[2])],
];
const report = [];
for (const [name, bytes] of models) {
    const document = JSON.parse(bytes.subarray(20, 20 + bytes.readUInt32LE(12)).toString());
    assert.ok(
        [...(document.buffers || []), ...(document.images || [])].every(
            (resource) => !resource.uri,
        ),
        'Model resources must be embedded: ' + name,
    );
    await writeFile(new URL(name, output), bytes);
    report.push({ name, bytes: bytes.length, sha256: digest(bytes) });
}
await writeFile(
    new URL('report.json', audit),
    JSON.stringify({ sources, models: report }, null, 2),
);
console.log('Prepared room models:', JSON.stringify(report));
