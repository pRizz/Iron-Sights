import * as THREE from 'three';
import { plastic } from './geometry';

// Measured from the pinned console GLB's player-one socket face/rim.
const SOCKET = new THREE.Vector3(-0.066, 0.033, 0.09205979);
const SOCKET_RADIUS = 0.01243149;
const SEGMENTS = 48;

function roundedCylinder(radius: number, length: number, bevel: number, grip = false) {
    const profile = [new THREE.Vector2(0, 0), new THREE.Vector2(radius - bevel, 0)];
    for (let i = 1; i <= 6; i++) {
        const angle = (i / 6) * Math.PI * 0.5;
        profile.push(
            new THREE.Vector2(
                radius - bevel + bevel * Math.sin(angle),
                bevel * (1 - Math.cos(angle)),
            ),
        );
    }
    const bodyRings = grip ? 24 : 1;
    for (let i = 1; i <= bodyRings; i++)
        profile.push(new THREE.Vector2(radius, bevel + ((length - 2 * bevel) * i) / bodyRings));
    for (let i = 1; i <= 6; i++) {
        const angle = (i / 6) * Math.PI * 0.5;
        profile.push(
            new THREE.Vector2(
                radius - bevel + bevel * Math.cos(angle),
                length - bevel + bevel * Math.sin(angle),
            ),
        );
    }
    profile.push(new THREE.Vector2(0, length));
    const geometry = new THREE.LatheGeometry(profile, SEGMENTS);
    geometry.rotateX(Math.PI / 2);
    if (!grip) return geometry;

    // A shallow thumb saddle is part of the shell, with zero slope at its edges.
    const positions = geometry.attributes.position;
    for (let i = 0; i < positions.count; i++) {
        const x = positions.getX(i);
        const y = positions.getY(i);
        const z = positions.getZ(i);
        const radial = Math.hypot(x, y);
        const along = (z - 0.005) / 0.018;
        if (radial === 0 || along <= 0 || along >= 1 || y <= 0) continue;
        const depth = 0.0022 * Math.sin(Math.PI * along) ** 2 * (y / radial) ** 4;
        positions.setXY(i, x * (1 - depth / radial), y * (1 - depth / radial));
    }
    geometry.computeVertexNormals();
    // Welding seam normals avoids a lighting crease through the thumb recess.
    const normals = geometry.attributes.normal;
    for (let j = 0; j < profile.length; j++) {
        const last = SEGMENTS * profile.length + j;
        const normal = new THREE.Vector3()
            .fromBufferAttribute(normals, j)
            .add(new THREE.Vector3().fromBufferAttribute(normals, last))
            .normalize();
        normals.setXYZ(j, normal.x, normal.y, normal.z);
        normals.setXYZ(last, normal.x, normal.y, normal.z);
    }
    return geometry;
}

export function createControllerPlug(model: THREE.Group) {
    const plug = new THREE.Group();
    plug.name = 'ControllerPlug';
    plug.position.copy(SOCKET);
    model.add(plug);
    const shell = new THREE.Mesh(
        roundedCylinder(SOCKET_RADIUS, 0.029, 0.0008, true),
        plastic(0xa3a5a2),
    );
    // Sink the rear bevel into the socket: its full-radius ring meets the rim plane.
    shell.position.z = -0.0008;
    shell.name = 'PlugShell';
    const collar = new THREE.Mesh(roundedCylinder(0.0065, 0.009, 0.001), plastic(0x90928f));
    collar.position.z = 0.0275;
    collar.name = 'PlugCableCollar';
    const relief = new THREE.Mesh(roundedCylinder(0.0038, 0.012, 0.0007), plastic(0x111213));
    relief.position.z = 0.0355;
    relief.name = 'PlugStrainRelief';
    for (const mesh of [shell, collar, relief]) {
        mesh.castShadow = mesh.receiveShadow = true;
        plug.add(mesh);
    }
    return {
        exit: plug.localToWorld(new THREE.Vector3(0, 0, 0.0468)).toArray(),
        lead: plug.localToWorld(new THREE.Vector3(0, 0, 0.0618)).toArray(),
    };
}
