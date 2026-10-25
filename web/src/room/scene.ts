import * as THREE from 'three';
import { box, disposeScene } from './geometry';
import { createConsole } from './console';
import { createCartridge } from './cartridge';
import { createTelevision } from './television';
import { furnishRoom } from './environment';
import { cameraPose, insertionPose, smooth, retreatFlight } from './cinematic';
import { createController } from './controller';
import { batchStaticProps } from './batch';
import { advanceQuality } from './quality';
import { easeOut, roomDuration, type RoomFlow } from './flow';
import { ROOM_FOV, TV_SCREEN } from './framing';
import { pivotRotation } from './pivot';
import { loadRoomModels } from './models';

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

export async function createRoomScene(canvas: HTMLCanvasElement) {
    const models = await loadRoomModels();
    const scene = new THREE.Scene();
    scene.add(models.console, models.controller);
    let maybeRenderer: THREE.WebGLRenderer | undefined;
    try {
        maybeRenderer = new THREE.WebGLRenderer({
            canvas,
            antialias: true,
            powerPreference: 'low-power',
        });
        return initializeScene(canvas, maybeRenderer, scene, models);
    } catch (error) {
        disposeScene(scene);
        maybeRenderer?.dispose();
        maybeRenderer?.forceContextLoss();
        throw error;
    }
}

function initializeScene(
    canvas: HTMLCanvasElement,
    renderer: THREE.WebGLRenderer,
    scene: THREE.Scene,
    models: Awaited<ReturnType<typeof loadRoomModels>>,
) {
    renderer.setPixelRatio(Math.min(devicePixelRatio, 1.5));
    renderer.outputColorSpace = THREE.SRGBColorSpace;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.1;
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFShadowMap;
    renderer.shadowMap.autoUpdate = false;
    scene.background = new THREE.Color(0x080a0e);
    scene.fog = new THREE.FogExp2(0x080a0e, 0.065);
    const camera = new THREE.PerspectiveCamera(ROOM_FOV, 1, 0.05, 35);
    const disposeEnvironment = furnishRoom(scene, renderer);
    const consoleProp = createConsole(scene, models.console);
    createController(scene, models.controller);
    const cartridge = createCartridge(scene);
    const tv = createTelevision(scene);
    // Larger invisible hit surfaces retain the physical switches' visual size.
    const hitMaterial = new THREE.MeshBasicMaterial({ visible: false });
    const powerHit = consoleProp.powerHit;
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
    let idlePitch = 0;
    let insertionPitch = 0;
    let dragging = false;
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
            if (!reduced && !dragging) idleAngle += delta * 0.3;
            cartridge.position.copy(floating);
            cartridge.position.y += reduced ? 0 : Math.sin(seconds * 1.4) * 0.035;
            cartridge.rotation.set(idlePitch, idleAngle, 0);
        } else if (state.phase === 'inserting') {
            if (previousPhase !== 'inserting') {
                insertionAngle = Math.atan2(Math.sin(idleAngle), Math.cos(idleAngle));
                insertionPitch = idlePitch;
            }
            const progress = state.elapsed / roomDuration('inserting', reduced);
            const pose = insertionPose(progress, insertionAngle);
            cartridge.position.set(...pose.position);
            cartridge.rotation.set(insertionPitch * (1 - smooth(progress / 0.55)), pose.yaw, 0);
        } else {
            cartridge.position.copy(inserted);
            cartridge.rotation.set(0, 0, 0);
        }
        const powered = ['powering', 'flying', 'complete', 'retreating'].includes(state.phase);
        const powerProgress =
            state.phase === 'powering'
                ? easeOut(state.elapsed / roomDuration('powering', reduced))
                : state.phase === 'retreating'
                  ? 1 - smooth(state.elapsed / roomDuration('retreating', reduced))
                  : powered
                    ? 1
                    : 0;
        consoleProp.setPower(powerProgress);
        consoleProp.ledMaterial.emissiveIntensity = powered ? 2 : 0;
        tv.glow.intensity = 12 + (reduced ? 0 : Math.sin(seconds * 23) * 0.12);
        tv.signal.uniforms.time.value = reduced ? 0 : seconds;
        tv.signal.uniforms.motion.value = reduced ? 0 : 1;
        tv.signal.uniforms.power.value = reduced ? (powered ? 1 : 0) : powerProgress;
        const flight =
            state.phase === 'flying'
                ? state.elapsed / roomDuration('flying', reduced)
                : state.phase === 'retreating'
                  ? retreatFlight(state.elapsed)
                  : state.phase === 'complete'
                    ? 1
                    : 0;
        const pose = cameraPose(reduced ? 0 : flight, camera.aspect);
        camera.position.set(...pose.position);
        camera.lookAt(...pose.look);
        camera.updateMatrixWorld();
        if (flight >= 1 && !reduced) {
            const corners = [-1, 1].flatMap((x) =>
                [-1, 1].map((y) =>
                    new THREE.Vector3(...TV_SCREEN.center)
                        .add(
                            new THREE.Vector3(
                                (x * TV_SCREEN.width) / 2,
                                (y * TV_SCREEN.height) / 2,
                                0,
                            ),
                        )
                        .project(camera)
                        .toArray(),
                ),
            );
            canvas.dataset.roomScreenCorners = JSON.stringify(corners);
        }
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
            state.phase === 'retreating' ||
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
        canvas.dataset.roomYaw = String(cartridge.rotation.y);
        canvas.dataset.roomPitch = String(cartridge.rotation.x);
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
        dragging(active: boolean) {
            dragging = active;
        },
        pivot(dx: number, dy: number) {
            const rotation = pivotRotation(
                idleAngle,
                idlePitch,
                dx,
                dy,
                Math.max(1, width),
                Math.max(1, height),
            );
            idleAngle = rotation.yaw;
            idlePitch = rotation.pitch;
        },
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
