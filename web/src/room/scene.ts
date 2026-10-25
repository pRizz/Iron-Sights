import * as THREE from 'three';
import { box, disposeScene } from './geometry';
import { createConsole } from './console';
import { createCartridge } from './cartridge';
import { createTelevision } from './television';
import { furnishRoom } from './environment';
import { cameraPose, insertionPose } from './cinematic';
import { createController } from './controller';
import { batchStaticProps } from './batch';
import { advanceQuality } from './quality';
import { easeOut, roomDuration, type RoomFlow } from './flow';

export type RoomTarget = 'cartridge' | 'power' | 'controls';

function paintScreen(
    tv: ReturnType<typeof createTelevision>,
    seconds: number,
    label: string,
    powered: boolean,
    reduced: boolean,
) {
    const ctx = tv.context;
    ctx.fillStyle = 'rgb(12,27,112)';
    ctx.fillRect(0, 0, 640, 480);
    ctx.fillStyle = '#d0d9ff';
    ctx.font = '23px monospace';
    ctx.textAlign = 'left';
    ctx.fillText('VIDEO 3', 42, 60);
    ctx.textAlign = 'center';
    ctx.font = '26px monospace';
    ctx.fillText(powered ? 'SIGNAL FOUND' : 'NO SIGNAL', 320, 220);
    ctx.font = '17px monospace';
    ctx.fillStyle = '#aebdea';
    ctx.fillText(label.toUpperCase().slice(0, 52), 320, 270);
    ctx.font = '14px monospace';
    ctx.fillText('F2  CONTROLS', 320, 420);
    tv.texture.needsUpdate = true;
}

export function createRoomScene(canvas: HTMLCanvasElement) {
    const renderer = new THREE.WebGLRenderer({
        canvas,
        antialias: true,
        powerPreference: 'low-power',
    });
    try {
        return initializeScene(canvas, renderer);
    } catch (error) {
        renderer.dispose();
        renderer.forceContextLoss();
        throw error;
    }
}

function initializeScene(canvas: HTMLCanvasElement, renderer: THREE.WebGLRenderer) {
    renderer.setPixelRatio(Math.min(devicePixelRatio, 1.5));
    renderer.outputColorSpace = THREE.SRGBColorSpace;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.1;
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    renderer.shadowMap.autoUpdate = false;
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x080a0e);
    scene.fog = new THREE.FogExp2(0x080a0e, 0.065);
    const camera = new THREE.PerspectiveCamera(42, 1, 0.05, 35);
    const disposeEnvironment = furnishRoom(scene, renderer);
    const consoleProp = createConsole(scene);
    createController(scene);
    const cartridge = createCartridge(scene);
    const tv = createTelevision(scene);
    // Larger invisible hit surfaces retain the physical switches' visual size.
    const hitMaterial = new THREE.MeshBasicMaterial({ visible: false });
    const powerHit = box(consoleProp.group, [0.4, 0.16, 0.35], [-0.57, 0.35, 0.28], hitMaterial);
    powerHit.name = 'power';
    const controlsHit = box(tv.knob, [0.2, 0.2, 0.2], [0, 0, 0], hitMaterial);
    controlsHit.name = 'controls';
    batchStaticProps(scene, [cartridge, consoleProp.power, tv.knob]);
    const raycaster = new THREE.Raycaster();
    const floating = new THREE.Vector3(-0.2, 1.35, 1.65);
    const inserted = new THREE.Vector3(-0.2, 0.66, 0.98);
    let seconds = 0;
    let lastScreen = -1;
    let idleAngle = 0.28;
    let insertionAngle = idleAngle;
    let previousPhase = 'empty';
    let width = 0;
    let height = 0;
    let frames = 0;
    let submittedMS = 0;
    let quality = { scale: 1, slowFrames: 0 };

    function render(
        state: RoomFlow,
        delta: number,
        reduced: boolean,
        label: string,
        frameMS = delta * 1000,
    ): void {
        if (scene.userData.assetError) throw new Error(scene.userData.assetError);
        if (scene.userData.artworkFormat) canvas.dataset.roomArtwork = scene.userData.artworkFormat;
        seconds += delta;
        const bounds = canvas.getBoundingClientRect();
        const previousScale = quality.scale;
        if (frames > 5) {
            const nextQuality = advanceQuality(quality, frameMS);
            if (nextQuality.scale !== quality.scale) {
                renderer.setPixelRatio(Math.min(devicePixelRatio, 1.5) * nextQuality.scale);
                width = 0;
                scene.traverse((object) => {
                    if (!(object instanceof THREE.SpotLight)) return;
                    object.shadow.map?.dispose();
                    object.shadow.map = null;
                    object.shadow.mapSize.set(512, 512);
                });
                renderer.shadowMap.needsUpdate = true;
            }
            quality = nextQuality;
        }
        if (bounds.width !== width || bounds.height !== height) {
            width = Math.max(1, bounds.width);
            height = Math.max(1, bounds.height);
            renderer.setSize(width, height, false);
            camera.aspect = width / height;
            camera.updateProjectionMatrix();
        }
        if (state.phase === 'empty') {
            if (!reduced) idleAngle += delta * 0.3;
            cartridge.position.copy(floating);
            cartridge.position.y += reduced ? 0 : Math.sin(seconds * 1.4) * 0.035;
            cartridge.rotation.set(0, idleAngle, 0);
        } else if (state.phase === 'inserting') {
            if (previousPhase !== 'inserting')
                insertionAngle = Math.atan2(Math.sin(idleAngle), Math.cos(idleAngle));
            const pose = insertionPose(
                state.elapsed / roomDuration('inserting', reduced),
                insertionAngle,
            );
            cartridge.position.set(...pose.position);
            cartridge.rotation.set(0, pose.yaw, 0);
        } else {
            cartridge.position.copy(inserted);
            cartridge.rotation.set(0, 0, 0);
        }
        const powered = ['powering', 'flying', 'complete'].includes(state.phase);
        const powerProgress =
            state.phase === 'powering'
                ? easeOut(state.elapsed / roomDuration('powering', reduced))
                : powered
                  ? 1
                  : 0;
        consoleProp.power.position.z = 0.28 - powerProgress * 0.07;
        consoleProp.ledMaterial.emissiveIntensity = powered ? 2 : 0;
        tv.glow.intensity = 12 + (reduced ? 0 : Math.sin(seconds * 23) * 0.12);
        tv.signal.uniforms.time.value = reduced ? 0 : seconds;
        tv.signal.uniforms.motion.value = reduced ? 0 : 1;
        tv.signal.uniforms.power.value = reduced ? (powered ? 1 : 0) : powerProgress;
        const flight =
            state.phase === 'flying'
                ? state.elapsed / roomDuration('flying', reduced)
                : state.phase === 'complete'
                  ? 1
                  : 0;
        const pose = cameraPose(reduced ? 0 : flight, camera.aspect);
        camera.position.set(...pose.position);
        camera.lookAt(...pose.look);
        if (seconds - lastScreen > 1 / 15 || previousPhase !== state.phase) {
            paintScreen(tv, seconds, label, powered, reduced);
            lastScreen = seconds;
        }
        renderer.shadowMap.needsUpdate =
            quality.scale !== previousScale ||
            frames === 0 ||
            state.phase !== previousPhase ||
            state.phase === 'inserting' ||
            state.phase === 'powering' ||
            (state.phase === 'empty' && !reduced && frames % 3 === 0);
        const before = performance.now();
        renderer.render(scene, camera);
        if (frames >= 5) submittedMS += performance.now() - before;
        frames++;
        canvas.dataset.roomFrames = String(frames);
        canvas.dataset.roomSubmitMs = String(submittedMS / Math.max(1, frames - 5));
        canvas.dataset.roomDrawCalls = String(renderer.info.render.calls);
        canvas.dataset.roomTriangles = String(renderer.info.render.triangles);
        canvas.dataset.roomScale = String(quality.scale);
        previousPhase = state.phase;
    }

    function hit(clientX: number, clientY: number): RoomTarget | undefined {
        const rect = canvas.getBoundingClientRect();
        raycaster.setFromCamera(
            new THREE.Vector2(
                ((clientX - rect.left) / rect.width) * 2 - 1,
                -((clientY - rect.top) / rect.height) * 2 + 1,
            ),
            camera,
        );
        const hits = raycaster.intersectObjects([cartridge, powerHit, controlsHit], true);
        const maybeHit = hits[0]?.object;
        if (!maybeHit) return undefined;
        if (maybeHit === powerHit) return 'power';
        if (maybeHit === controlsHit) return 'controls';
        return 'cartridge';
    }

    return {
        render,
        hit,
        hover(target: RoomTarget | undefined) {
            for (const [object, active] of [
                [cartridge, target === 'cartridge'],
                [consoleProp.power, target === 'power'],
                [tv.knob, target === 'controls'],
            ] as const) {
                object.traverse((part) => {
                    if (!(part instanceof THREE.Mesh)) return;
                    const material = part.material;
                    if (material instanceof THREE.MeshStandardMaterial) {
                        material.emissive.setHex(active ? 0xc29a4f : 0);
                        material.emissiveIntensity = active ? 0.15 : 0;
                    }
                });
            }
        },
        dispose() {
            disposeScene(scene);
            disposeEnvironment();
            renderer.dispose();
            renderer.forceContextLoss();
        },
    };
}
